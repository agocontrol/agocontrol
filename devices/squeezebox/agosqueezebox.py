#! /usr/bin/env python

import base64
import re
from agoclient import AgoApp
from agoclient import agoproto
import pylmsserver
import pylmsplaylist
import pylmslibrary

class AgoSqueezebox(AgoApp):

    STATE_OFF = 0
    STATE_ON = 255
    STATE_STREAM = 50
    STATE_PLAY = 100
    STATE_STOP = 150
    STATE_PAUSE = 200

    def __init__(self):
        AgoApp.__init__(self)

        self.host = self.get_config_option(app='squeezebox', option='host', default_value='127.0.0.1')
        self.cli_port = int(self.get_config_option(app='squeezebox', option='cliport', default_value='9090'))
        self.html_port = int(self.get_config_option(app='squeezebox', option='htmlport', default_value='9000'))
        self.login = self.get_config_option(app='squeezebox', option='login', default_value='')
        self.passwd = self.get_config_option(app='squeezebox', option='password', default_value='')

        self.playlist = None
        self.server = None
        self.library = None
        self.players_states = {}

    def setup_app(self):
        # configure client
        self.connection.add_handler(self.message_handler)

        # connect to LMS
        self.server = None
        self.playlist = None
        self.library = None
        try:
            self.connect_to_server()
            self.connect_to_notifications()
        except Exception:
            self.log.exception('Failed to connect to server. Exit now')
            raise

        # add devices
        self.connection.add_device(self.host, 'squeezeboxserver')
        try:
            self.log.info('Discovering players:')
            for player in self.server.get_players():
                self.log.info("  Add player : %s[%s]" % (player.name, player.mac))
                self.connection.add_device(player.mac, "squeezebox")
        except Exception:
            self.log.exception('Error discovering players. Exit now')
            raise
        self.get_players_states()

    def cleanup_app(self):
        if self.playlist:
            self.playlist.stop()

    def connect_to_server(self):
        self.log.info('Connecting to server %s@%s:%d' % (self.login, self.host, self.cli_port))
        self.server = pylmsserver.LMSServer(self.host, self.cli_port, self.login, self.passwd)
        self.server.connect()

    def connect_to_notifications(self):
        self.log.info('Connecting to notifications handlers')
        self.library = pylmslibrary.LMSLibrary(self.host, self.cli_port, self.html_port, self.login, self.passwd)
        self.playlist = pylmsplaylist.LMSPlaylist(self.library, self.host, self.cli_port, self.login, self.passwd)

        # play, pause, stop, on, off, add, del, move, reload
        self.playlist.set_callbacks(
            self.play_callback,
            self.pause_callback,
            self.stop_callback,
            self.on_callback,
            self.off_callback,
            None,
            None,
            None,
            None,
        )
        self.playlist.start()

    def get_players_states(self):
        for player in self.server.get_players():
            if player.get_model() == 'http':
                #it's a stream. No control on it
                self.log.info('  Player %s[%s] is streaming' % (player.name, player.mac))
                self.players_states[player.mac] = self.STATE_STREAM
                self.emit_stream(player.mac)
            elif not player.get_is_on():
                #player is off
                self.log.info('  Player %s[%s] is off' % (player.name, player.mac))
                self.players_states[player.mac] = self.STATE_STOP
                self.emit_off(player.mac)
            else:
                #player is on
                mode = player.get_mode()
                if mode == 'stop':
                    self.log.info('  Player %s[%s] is stopped' % (player.name, player.mac))
                    self.players_states[player.mac] = self.STATE_STOP
                    self.emit_stop(player.mac)
                elif mode == 'play':
                    self.log.info('  Player %s[%s] is playing' % (player.name, player.mac))
                    self.players_states[player.mac] = self.STATE_PLAY
                    self.emit_play(player.mac, None)
                elif mode == 'pause':
                    self.log.info('  Player %s[%s] is paused' % (player.name, player.mac))
                    self.players_states[player.mac] = self.STATE_PAUSE
                    self.emit_pause(player.mac)

            #emit media infos
            self.emit_media_infos(player.mac, None)

    def message_handler(self, internalid, content):
        """
        Message handler

        Note:
            Received state values:
                0 : OFF
                255 : ON
                25 : STREAM
                50 : PLAYING
                100 : STOPPED
                150 : PAUSED
        """
        #check parameters
        if 'command' not in content:
            return agoproto.response_bad_parameters()

        if internalid == self.host:
            return self.handle_server_commands(internalid, content)
        return self.handle_player_commands(internalid, content)

    def handle_server_commands(self, internalid, content):
        """
        Handle server commands
        """
        if content['command'] == 'allon':
            self.log.info('Command ALLON: %s' % internalid)
            for player in self.get_players():
                player.on()
            return agoproto.response_success()

        if content['command'] == 'alloff':
            self.log.info('Command ALLOFF: %s' % internalid)
            for player in self.get_players():
                player.off()
            return agoproto.response_success()

        if content['command'] == 'displaymessage':
            if 'line1' in content and 'line2' in content and 'duration' in content:
                self.log.info('Command DISPLAYMESSAGE: %s' % internalid)
                for player in self.get_players():
                    player.display(content['line1'], content['line2'], content['duration'])
                return agoproto.response_success()

            self.log.error('Missing parameters to command DISPLAYMESSAGE')
            return agoproto.response_missing_parameters(data={'command': 'displaymessage', 'params': ['line1', 'line2', 'duration']})

        #unhandled command
        self.log.warning('Unhandled server command')
        return agoproto.response_unknown_command(message='Unhandled server command', data=content["command"])

    def handle_player_commands(self, internalid, content):
        """
        Handle player commands
        """
        # get player
        player = self.get_player(internalid)
        self.log.debug('Found player: %s' % player)

        if not player:
            self.log.error('Player %s not found!' % internalid)
            return agoproto.response_failed('Player "%s" not found!' % internalid)

        if content['command'] == 'on':
            self.log.info('Command ON: %s' % internalid)
            player.on()
            return agoproto.response_success()

        if content['command'] == 'off':
            self.log.info("Command OFF: %s" % internalid)
            player.off()
            return agoproto.response_success()

        if content['command'] == 'play':
            self.log.info('Command PLAY: %s' % internalid)
            player.play()
            return agoproto.response_success()

        if content['command'] == 'pause':
            self.log.info('Command PAUSE: %s' % internalid)
            player.pause()
            return agoproto.response_success()

        if content['command'] == 'stop':
            self.log.info('Command STOP: %s' % internalid)
            player.stop()
            return agoproto.response_success()

        if content['command'] == 'next':
            self.log.info('Command NEXT: %s' % internalid)
            player.next()
            return agoproto.response_success()

        if content['command'] == 'previous':
            self.log.info('Command PREVIOUS: %s' % internalid)
            player.prev()
            return agoproto.response_success()

        if content['command'] == 'setvolume':
            self.log.info('Command SETVOLUME: %s' % internalid)
            if 'volume' in content:
                player.set_volume(content['volume'])
                return agoproto.response_success()

            self.log.error('Missing parameter "volume" to command SETVOLUME')
            return agoproto.response_missing_parameters(data={'command': 'setvolume', 'params': ['volume']})

        if content['command'] == 'displaymessage':
            if 'line1' in content and 'line2' in content and 'duration' in content:
                self.log.info('Command DISPLAYMESSAGE: %s' % internalid)
                player.display(content['line1'], content['line2'], content['duration'])
                return agoproto.response_success()

            self.log.error('Missing parameters to command DISPLAYMESSAGE')
            return agoproto.response_missing_parameters(data={'command': 'displaymessage', 'params': ['line1', 'line2', 'duration']})

        if content['command'] == 'mediainfos':
            infos = self.get_media_infos(internalid, None)
            self.log.info(infos)
            return agoproto.response_success(infos)

        #unhandled player command
        self.log.warning('Unhandled player command')
        return agoproto.response_unknown_command(message='Unhandled player command', data=content["command"])

    def get_players(self):
        return self.playlist.get_server().get_players()

    def get_player(self, player_id):
        return self.playlist.get_server().get_player(player_id)

    def get_media_infos(self, internalid, infos):
        self.log.debug('get_media_infos for %s' % internalid)
        if not internalid:
            self.log.error("Unable to emit mediainfos of not specified device")
            return False

        if not infos:
            #get player infos
            infos = self.playlist.get_current_song(internalid)

            if not infos:
                self.log.error('Unable to find infos of internalid %s' % internalid)
                return False
        self.log.debug('get_media_infos: %s' % infos)

        #prepare data
        title = 'Unknown'
        album = 'Unknown'
        artist = 'Unknown'
        cover_data = None
        if 'remote' in infos and infos['remote'] == '1':
            #get cover from online service
            cover_data = self.library.get_remote_cover(infos['artwork_url'])
            if 'album' in infos and 'artist' in infos and 'title' in infos:
                title = infos['title']
                album = infos['album']
                artist = infos['artist']
            elif 'current_title' in infos:
                # split all infos from current_title field
                # format: The Greatest by Cat Power from The Greatest
                pattern = re.compile(u'(.*) by (.*) from (.*)')
                matches = pattern.findall(infos['current_title'])
                if len(matches) > 0 and len(matches[0]) == 3:
                    (title, artist, album) = matches[0]
        else:
            #get cover from local source
            filename = 'cover_%s.jpg' % ''.join(x for x in internalid if x.isalnum())
            if 'album_id' in infos and 'artwork_track_id' in infos:
                cover_data = self.library.get_cover(infos['album_id'], infos['artwork_track_id'], filename, (100, 100))
            if 'title' in infos:
                title = infos['title']
            if 'album' in infos:
                album = infos['album']
            if 'artist' in infos:
                artist = infos['artist']
        cover_b64 = None
        if cover_data:
            cover_b64 = base64.b64encode(cover_data)
            self.log.debug('cover_b64 %d' % len(cover_b64))

        #and fill returned data
        return {
            'title':title,
            'album':album,
            'artist':artist,
            'cover':cover_b64
        }

    def emit_play(self, internalid, song_infos):
        self.log.debug('emit PLAY for %s' % internalid)
        self.log.debug('emit PLAY song infos %s' % song_infos)
        self.connection.emit_event(internalid, 'event.device.statechanged', str(self.STATE_PLAY), "")

    def emit_stop(self, internalid):
        self.log.debug('emit STOP for %s' % internalid)
        self.connection.emit_event(internalid, 'event.device.statechanged', str(self.STATE_STOP), "")

    def emit_pause(self, internalid):
        self.log.debug('emit PAUSE for %s' % internalid)
        self.connection.emit_event(internalid, 'event.device.statechanged', str(self.STATE_PAUSE), "")

    def emit_on(self, internalid):
        self.log.debug('emit ON for %s' % internalid)
        self.connection.emit_event(internalid, 'event.device.statechanged', str(self.STATE_ON), "")

    def emit_off(self, internalid):
        self.log.debug('emit OFF for %s' % internalid)
        self.connection.emit_event(internalid, 'event.device.statechanged', str(self.STATE_OFF), "")

    def emit_stream(self, internalid):
        self.log.debug('emit STREAM for %s' % internalid)
        self.connection.emit_event(internalid, 'event.device.statechanged', str(self.STATE_STREAM), "")

    def emit_media_infos(self, internalid, infos):
        self.log.debug('emit MEDIAINFOS')
        data = self.get_media_infos(internalid, infos)
        self.connection.emit_event_raw(internalid, 'event.device.mediainfos', data)

    def play_callback(self, player_id, song_infos, playlist_index):
        #if player is off or player is already playing, kick this callback
        self.log.debug('callback PLAY: %s' % player_id)
        self.log.debug('callback PLAY: song %s' % song_infos)
        self.log.debug('callback PLAY: playlist index %s' % playlist_index)
        if player_id in self.players_states and self.players_states[player_id] != self.STATE_PLAY and self.players_states[player_id] != self.STATE_OFF and self.players_states[player_id] != self.STATE_STREAM:
            self.players_states[player_id] = self.STATE_PLAY
            self.emit_play(player_id, song_infos)
        #always emit media infos, even for streaming
        self.emit_media_infos(player_id, song_infos)

    def stop_callback(self, player_id):
        #if player is off, kick this callback
        self.log.debug('callback STOP: %s' % player_id)
        if player_id in self.players_states and self.players_states[player_id] != self.STATE_STOP and self.players_states[player_id] != self.STATE_OFF and self.players_states[player_id] != self.STATE_STREAM:
            self.players_states[player_id] = self.STATE_STOP
            self.emit_stop(player_id)

    def pause_callback(self, player_id):
        #if player is off, kick this callback
        self.log.debug('callback PAUSE: %s' % player_id)
        if player_id in self.players_states and self.players_states[player_id] != self.STATE_PAUSE and self.players_states[player_id] != self.STATE_OFF and self.players_states[player_id] != self.STATE_STREAM:
            self.players_states[player_id] = self.STATE_PAUSE
            self.emit_pause(player_id)

    def on_callback(self, player_id):
        self.log.debug('callback ON: %s' % player_id)
        if player_id in self.players_states and self.players_states[player_id] != self.STATE_ON and self.players_states[player_id] != self.STATE_STREAM:
            self.players_states[player_id] = self.STATE_ON
            self.emit_on(player_id)

    def off_callback(self, player_id):
        self.log.debug('callback OFF: %s' % player_id)
        if player_id in self.players_states and self.players_states[player_id] != self.STATE_OFF and self.players_states[player_id] != self.STATE_STREAM:
            self.players_states[player_id] = self.STATE_OFF
            self.emit_off(player_id)


if __name__ == "__main__":
    AgoSqueezebox().main()

