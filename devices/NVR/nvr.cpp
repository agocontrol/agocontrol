#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <syslog.h>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>
#include <opencv2/opencv.hpp>
#include <queue>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>

#include "agoapp.h"
#include "base64.h"
#include "frameprovider.h"
#include "agoutils.h"

#ifndef VIDEOMAPFILE
#define VIDEOMAPFILE "maps/videomap.json"
#endif

#ifndef RECORDINGSDIR
#define RECORDINGSDIR "recordings/"
#endif

using namespace agocontrol;
using namespace cv;
using namespace std; // XXX: Ugly. Proper cleanup is in qpid-removal branch.
namespace pt = boost::posix_time;
namespace fs = boost::filesystem;

typedef struct TBox {
    int minX;
    int maxX;
    int minY;
    int maxY;
} Box;

class AgoSurveillance: public AgoApp {
private:
    Json::Value videomap;
    boost::mutex videomapMutex;

    void eventHandler(const std::string& subject , const Json::Value& content);
    Json::Value commandHandler(const Json::Value& content);

    bool stopProcess;
    bool stopTimelapses;
    int restartDelay;
    void setupApp();
    void cleanupApp();

    //video
    void getRecordings(std::string type, Json::Value& list, std::string process);
    std::string getDateTimeString(bool date, bool time, bool withSeparator=true, std::string fieldSeparator="_");
    std::map<std::string, AgoFrameProvider*> frameProviders;
    AgoFrameProvider* getFrameProvider(std::string uri);
    boost::mutex frameProvidersMutex;
    bool frameToString(Mat& frame, std::string& image);

    //stream
    void fillStream(Json::Value& stream, const Json::Value& content);

    //timelapse
    std::map<std::string, boost::thread*> timelapseThreads;
    void fillTimelapse(Json::Value& timelapse, const Json::Value& content);
    void timelapseFunction(std::string internalid, Json::Value timelapse);
    void restartTimelapses();
    void launchTimelapses();
    void launchTimelapse(std::string internalid, const Json::Value& timelapse);
    void stopTimelapse(std::string internalid);

    //motion
    std::map<std::string, boost::thread*> motionThreads;
    void fillMotion(Json::Value& timelapse, const Json::Value& content);
    void motionFunction(std::string internalid, Json::Value timelapse);
    void launchMotions();
    void launchMotion(std::string internalid, Json::Value& motion);
    void stopMotion(std::string internalid);

public:

    AGOAPP_CONSTRUCTOR_HEAD(AgoSurveillance)
        , stopProcess(false)
        , stopTimelapses(false)
        , restartDelay(12) {}
};


/**
 * Return frame provider. If provider doesn't exist for specified uri, new one is created
 * and returned
 */
AgoFrameProvider* AgoSurveillance::getFrameProvider(std::string uri)
{
    AgoFrameProvider* out = NULL;

    boost::lock_guard<boost::mutex> lock(frameProvidersMutex);

    //search existing frame provider
    std::map<std::string, AgoFrameProvider*>::iterator item = frameProviders.find(uri);
    if( item==frameProviders.end() )
    {
        AGO_DEBUG() << "Create new frame provider '" << uri << "'";
        //frame provider doesn't exist for specified uri, create new one
        AgoFrameProvider* provider = new AgoFrameProvider(uri);
        if( !provider->start() )
        {
            //unable to start frame provider, uri is valid?
            return NULL;
        }
        frameProviders[uri] = provider;
        out = provider;
    }
    else
    {
        AGO_DEBUG() << "Frame provider already exists for '" << uri << "'";
        out = item->second;
    }

    return out;
}

/**
 * Convert opencv frame to string
 * @info encode to jpeg format (quality 90)
 */
bool AgoSurveillance::frameToString(Mat& frame, std::string& image)
{
    std::vector<unsigned char> buffer;

    //prepare encoder parameters
    std::vector<int> params;
    params.push_back(CV_IMWRITE_JPEG_QUALITY);
    params.push_back(90);

    //encode image
    if( imencode(".jpg", frame, buffer, params) )
    {
        image = std::string(buffer.begin(), buffer.end());
        return true;
    }

    return false;
}

/**
 * Fill stream map
 * @param stream: map to fill
 * @param content: map from request used to prefill output map
 */
void AgoSurveillance::fillStream(Json::Value& stream, const Json::Value &content)
{
#if 0
    if( content==NULL )
    {
        //fill with default values
        stream["process"] = "";
        stream["uri"] = "";
    }
    else
    {
#endif
        //fill with specified content
        if( content.isMember("process") )
            stream["process"] = content["process"].asString();
        else
            stream["process"] = "";

        if( content.isMember("uri") )
            stream["uri"] = content["uri"].asString();
        else
            stream["uri"] = "";
#if 0
    }
#endif
}

/**
 * Timelapse function (threaded)
 */
void AgoSurveillance::timelapseFunction(std::string internalid, Json::Value timelapse)
{
    //init video reader (provider and consumer)
    std::string timelapseUri = timelapse["uri"].asString();
    AgoFrameProvider* provider = getFrameProvider(timelapseUri);
    if( provider==NULL )
    {
        //no frame provider
        AGO_ERROR() << "Timelapse '" << internalid << "': stopped because no provider available";
        return;
    }
    AgoFrameConsumer consumer;
    provider->subscribe(&consumer);

    AGO_DEBUG() << "Timelapse '" << internalid << "': started";

    //init video writer
    bool fileOk = false;
    int inc = 0;
    fs::path filepath;
    while( !fileOk )
    {
        std::string name = timelapse["name"].asString();
        std::stringstream filename;
        filename << RECORDINGSDIR;
        filename << "timelapse_";
        filename << internalid << "_";
        filename << getDateTimeString(true, false, false);
        if( inc>0 )
        {
            filename << "_" << inc;
        }
        filename << ".avi";
        filepath = getLocalStatePath(filename.str());
        if( fs::exists(filepath) )
        {
            //file already exists
            inc++;
        }
        else
        {
            fileOk = true;
        }
    }
    AGO_DEBUG() << "Record into '" << filepath.c_str() << "'";
    std::string codec = timelapse["codec"].asString();
    int fourcc = CV_FOURCC('F', 'M', 'P', '4');
    if( codec.length()==4 )
    {
        fourcc = CV_FOURCC(codec[0], codec[1], codec[2], codec[3]);
    }
    int fps = 24;
    VideoWriter recorder(filepath.c_str(), fourcc, fps, provider->getResolution());
    if( !recorder.isOpened() )
    {
        //XXX emit error?
        AGO_ERROR() << "Timelapse '" << internalid << "': unable to open recorder";
        return;
    }

    try
    {
        int now = (int)(time(NULL));
        int last = 0;
        Mat frame;
        while( !stopProcess && !stopTimelapses )
        {
            if( provider->isRunning() )
            {
                //get frame in any case (to empty queue)
                frame = consumer.popFrame(&boost::this_thread::sleep_for);
                recorder << frame;

                //TODO handle recording fps using timelapse["fps"] param.
                //For now it records at 1 fps to minimize memory leak of cv::VideoWriter
                if( now!=last )
                {
                    //need to copy frame to alter it
                    Mat copiedFrame = frame.clone();

                    //add text
                    std::stringstream stream;
                    stream << getDateTimeString(true, true, true, " ");
                    stream << " - " << timelapse["name"].asString();
                    std::string text = stream.str();

                    try
                    {
                        putText(copiedFrame, text.c_str(), Point(20,20), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0,0,0), 4, CV_AA);
                        putText(copiedFrame, text.c_str(), Point(20,20), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(255,255,255), 1, CV_AA);
                    }
                    catch(cv::Exception& e)
                    {
                        AGO_ERROR() << "Timelapse '" << internalid << "': opencv exception occured " << e.what();
                    }

                    //record frame
                    recorder << copiedFrame;
                    last = now;
                }
            }

            //update current time
            now = (int)(time(NULL));
    
            //check if thread has been interrupted
            boost::this_thread::interruption_point();
        }
    }
    catch(boost::thread_interrupted &e)
    {
        AGO_DEBUG() << "Timelapse '" << internalid << "': thread interrupted";
    }
    
    //close all
    AGO_DEBUG() << "Timelapse '" << internalid << "': close recorder";
    if( recorder.isOpened() )
    {
        recorder.release();
    }
    
    //unsubscribe from provider
    AGO_DEBUG() << "Timelapse '" << internalid << "': unsubscribe from frame provider";
    provider->unsubscribe(&consumer);

    AGO_DEBUG() << "Timelapse '" << internalid << "': stopped";
}

/**
 * Fill timelapse map
 * @param timelapse: map to fill
 * @param content: map from request used to prefill output map
 */
void AgoSurveillance::fillTimelapse(Json::Value& timelapse, const Json::Value& content)
{
#if 0
    if( content==NULL )
    {
        //fill with default values
        timelapse["process"] = "";
        timelapse["name"] = "noname";
        timelapse["uri"] = "";
        timelapse["fps"] = 1;
        timelapse["codec"] = "FMP4";
        timelapse["enabled"] = true;
    }
    else
    {
#endif
        //fill with specified content
        if( content.isMember("process") )
            timelapse["process"] = content["process"].asString();
        else
            timelapse["process"] = "";

        if( content.isMember("name") )
            timelapse["name"] = content["name"].asString();
        else
            timelapse["name"] = "noname";

        if( content.isMember("uri") )
            timelapse["uri"] = content["uri"].asString();
        else
            timelapse["uri"] = "";

        if( content.isMember("fps") )
            timelapse["fps"] = content["fps"].asInt();
        else
            timelapse["fps"] = 1;

        if( content.isMember("codec") )
            timelapse["codec"] = content["codec"].asString();
        else
            timelapse["codec"] = "FMP4";

        if( content.isMember("enabled") )
            timelapse["enabled"] = content["enabled"].asBool();
        else
            timelapse["enabled"] = true;
#if 0
    }
#endif
}

/**
 * Restart timelapses
 */
void AgoSurveillance::restartTimelapses()
{
    //stop current timelapses
    stopTimelapses = true;
    for( std::map<std::string, boost::thread*>::iterator it=timelapseThreads.begin(); it!=timelapseThreads.end(); it++ )
    {
        stopTimelapse(it->first);
    }

    //then restart them all
    stopTimelapses = false;
    launchTimelapses();
}

/**
 * Launch all timelapses
 */
void AgoSurveillance::launchTimelapses()
{
    const Json::Value& timelapses = videomap["timelapses"];
    for( auto it = timelapses.begin(); it!=timelapses.end(); it++ )
    {
        std::string internalid = it.name();
        const Json::Value& timelapse = *it;
        launchTimelapse(internalid, timelapse);
    }
}

/**
 * Launch specified timelapse
 */
void AgoSurveillance::launchTimelapse(std::string internalid, const Json::Value& timelapse)
{
    //create timelapse device
    agoConnection->addDevice(internalid, "timelapse");
    
    AGO_DEBUG() << "Launch timelapse '" << internalid << "'";
    if( timelapse["enabled"].asBool()==true )
    {
        boost::thread* thread = new boost::thread(boost::bind(&AgoSurveillance::timelapseFunction, this, internalid, timelapse));
        timelapseThreads[internalid] = thread;
    }
    else
    {
        AGO_DEBUG() << " -> not launch because timelapse is disabled";
    }
}

/**
 * Stop timelapse thread
 */
void AgoSurveillance::stopTimelapse(std::string internalid)
{
    //stop thread
    timelapseThreads[internalid]->interrupt();
    timelapseThreads[internalid]->join();

    //remove thread from list
    timelapseThreads.erase(internalid);
}

/**
 * Detect motion function
 * @return number of changes
 */
inline int detectMotion(const Mat& __motion, Mat& result, Box& area, int maxDeviation, Scalar& color)
{
    //calculate the standard deviation
    Scalar mean, stddev;
    meanStdDev(__motion, mean, stddev);
    //AGO_DEBUG() << "stddev[0]=" << stddev[0];

    //if not to much changes then the motion is real (neglect agressive snow, temporary sunlight)
    AGO_DEBUG() << "stddev=" << stddev[0];
    if( stddev[0]<maxDeviation )
    {
        int numberOfChanges = 0;
        int minX = __motion.cols, maxX = 0;
        int minY = __motion.rows, maxY = 0;

        // loop over image and detect changes
        for( int j=area.minY; j<area.maxY; j+=2 ) // height
        {
            for( int i=area.minX; i<area.maxX; i+=2 ) // width
            {
                //check if at pixel (j,i) intensity is equal to 255
                //this means that the pixel is different in the sequence
                //of images (prev_frame, current_frame, next_frame)
                if( static_cast<int>(__motion.at<uchar>(j,i))==255 )
                {
                    numberOfChanges++;
                    if( minX>i ) minX = i;
                    if( maxX<i ) maxX = i;
                    if( minY>j ) minY = j;
                    if( maxY<j ) maxY = j;
                }
            }
        }

        if( numberOfChanges )
        {
            //check if not out of bounds
            if( minX-10>0) minX -= 10;
            if( minY-10>0) minY -= 10;
            if( maxX+10<result.cols-1 ) maxX += 10;
            if( maxY+10<result.rows-1 ) maxY += 10;

            // draw rectangle round the changed pixel
            Point x(minX, minY);
            Point y(maxX, maxY);
            Rect rect(x, y);
            rectangle(result, rect, color, 2);
        }

        return numberOfChanges;
    }

    return 0;
}

/**
 * Motion function (threaded)
 * Based on Cédric Verstraeten source code: https://github.com/cedricve/motion-detection/blob/master/motion_src/src/motion_detection.cpp
 */
void AgoSurveillance::motionFunction(std::string internalid, Json::Value motion)
{
    AGO_DEBUG() << "Motion '" << internalid << "': started";

    //init video reader (provider and consumer)
    std::string motionUri = motion["uri"].asString();
    AgoFrameProvider* provider = getFrameProvider(motionUri);
    if( provider==NULL )
    {
        //no frame provider
        AGO_ERROR() << "Motion '" << internalid << "': stopped because no provider available";
        return;
    }
    AgoFrameConsumer consumer;
    provider->subscribe(&consumer);
    Size resolution = provider->getResolution();
    int fps = provider->getFps();
    AGO_DEBUG() << "Motion '" << internalid << "': fps=" << fps;

    /*VideoCapture _capture = VideoCapture(motionUri);
    Size resolution = Size(_capture.get(CV_CAP_PROP_FRAME_WIDTH), _capture.get(CV_CAP_PROP_FRAME_HEIGHT));
    AGO_DEBUG() << "resolution=" << resolution;
    int fps = _capture.get(CV_CAP_PROP_FPS);
    AGO_DEBUG() << "fps=" << fps;*/

    //init buffer
    //unsigned int maxBufferSize = motion["bufferduration"].asInt() * fps;
    std::queue<Mat> buffer;

    //get frames and convert to gray
    Mat prevFrame, currentFrame, nextFrame, result, tempFrame;
    if( provider->isRunning() )
    //if( _capture.isOpened() )
    {
        prevFrame = consumer.popFrame(&boost::this_thread::sleep_for);
        //_capture >> prevFrame;
        cvtColor(prevFrame, prevFrame, CV_RGB2GRAY);
        AGO_DEBUG() << "prevframe ok";

        currentFrame = consumer.popFrame(&boost::this_thread::sleep_for);
        //_capture >> currentFrame;
        cvtColor(currentFrame, currentFrame, CV_RGB2GRAY);
        AGO_DEBUG() << "currentframe ok";

        nextFrame = consumer.popFrame(&boost::this_thread::sleep_for);
        //_capture >> nextFrame;
        cvtColor(nextFrame, nextFrame, CV_RGB2GRAY);
        AGO_DEBUG() << "nextframe ok";
    }
    else
    {
        AGO_DEBUG() << "not opened";
    }

    //other declarations
    std::string name = motion["name"].asString();
    fs::path recordPath;
    int fourcc = CV_FOURCC('F', 'M', 'P', '4');
    int now = (int)time(NULL);
    int startup = now;
    int onDuration = motion["onduration"].asInt() - motion["bufferduration"].asInt();
    int recordDuration = motion["recordduration"].asInt();
    bool recordEnabled = motion["recordenabled"].asBool();
    bool isRecording, isTriggered = false;
    int triggerStart = 0;
    Mat d1, d2, _motion;
    int numberOfChanges = 0;
    Scalar color(0,0,255);
    int thereIsMotion = motion["sensitivity"].asInt();
    int maxDeviation = motion["deviation"].asInt();
    AGO_TRACE() << "Motion '" << internalid << "': maxdeviation=" << maxDeviation << " sensitivity=" << thereIsMotion;
    Mat kernelErode = getStructuringElement(MORPH_RECT, Size(2,2));
    Box area = {0, currentFrame.cols, 0, currentFrame.rows};
    AGO_TRACE() << "Motion '" << internalid << "': area minx=" << area.minX << " maxx=" << area.maxX << " miny=" << area.minY << " maxy=" << area.maxY;
    VideoWriter recorder("", 0, 1, Size(10,10)); //dummy recorder, unable to compile with empty constructor :S

    //debug purpose: display frame in window
    //namedWindow("Display window", WINDOW_AUTOSIZE );

    try
    {
        while( !stopProcess )
        {
            if( provider->isRunning() )
            {
                //get new frame
                prevFrame = currentFrame;
                currentFrame = nextFrame;
                nextFrame = consumer.popFrame(&boost::this_thread::sleep_for);
                //_capture >> nextFrame;
                result = nextFrame; //keep color copy
                cvtColor(nextFrame, nextFrame, CV_RGB2GRAY);

                //add text to frame (current time and motion name)
                /*stringstream stream;
                stream << getDateTimeString(true, true, true, " ");
                if( name.length()>0 )
                {
                    stream << " - " << name;
                }
                std::string text = stream.str();
                try
                {
                    putText(result, text.c_str(), Point(20,20), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0,0,0), 4, CV_AA);
                    putText(result, text.c_str(), Point(20,20), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(255,255,255), 1, CV_AA);
                }
                catch(cv::Exception& e)
                {
                    AGO_ERROR() << "Motion '" << internalid << "': opencv exception #1 occured " << e.what();
                }*/

                //handle buffer
                /*if( !isRecording )
                {
                    while( buffer.size()>=maxBufferSize )
                    {
                        //remove old frames
                        buffer.pop();
                    }
                    buffer.push(result);
                }*/

                //calc differences between the images and do AND-operation
                //threshold image, low differences are ignored (ex. contrast change due to sunlight)
                //try
                //{
                    absdiff(prevFrame, nextFrame, d1);
                    absdiff(nextFrame, currentFrame, d2);
                    bitwise_and(d1, d2, _motion);
                    //debug purpose: display frame in window
                    //imshow("Display window", nextFrame);
                    threshold(_motion, _motion, 35, 255, CV_THRESH_BINARY);
                    erode(_motion, _motion, kernelErode);
                /*}
                catch(cv::Exception& e)
                {
                    AGO_ERROR() << "Motion '" << internalid << "': opencv exception #2 occured " << e.what();
                }*/

                //debug purpose: display frame in window
                //imshow("Display window", _motion);
                
                //check if thread has been interrupted
                boost::this_thread::interruption_point();

                //update current time
                now = (int)time(NULL);

                //drop first 5 seconds for stabilization
                if( now<=(startup+5) )
                {
                    AGO_DEBUG() << "Wait for stabilization (" << (now-startup) << "s)";
                    continue;
                }

                //detect motion
                numberOfChanges = 0;
                //try
                //{
                    numberOfChanges = detectMotion(_motion, result, area, maxDeviation, color);
                    AGO_DEBUG() << numberOfChanges;
                /*}
                catch(cv::Exception& e)
                {
                    AGO_ERROR() << "Motion '" << internalid << "': opencv exception #3 occured " << e.what();
                }*/

                if( !isTriggered )
                {
                    //analyze motion
                    if( numberOfChanges>=thereIsMotion )
                    {
                        //save picture and send pictureavailable event
                        std::stringstream filename;
                        filename << "/tmp/" << internalid << ".jpg";
                        std::string picture = filename.str();
                        try
                        {
                            imwrite(picture.c_str(), result);
                            Json::Value content;
                            content["filename"] = picture;
                            agoConnection->emitEvent(internalid, "event.device.pictureavailable", content);
                        }
                        catch(...)
                        {
                            AGO_ERROR() << "Motion '" << internalid << "': Unable to write motion picture '" << picture << "' to disk";
                        }
                
                        //prepare recorder
                        if( recordEnabled )
                        {
                            filename.str("");
                            filename << RECORDINGSDIR;
                            filename << "motion_";
                            filename << internalid << "_";
                            filename << getDateTimeString(true, true, false, "_");
                            filename << ".avi";
                            recordPath = getLocalStatePath(filename.str());
                            AGO_DEBUG() << "Motion '" << internalid << "': record to " << recordPath.c_str();

                            try
                            {
                                recorder.open(recordPath.c_str(), fourcc, fps, resolution);
                                if( !recorder.isOpened() )
                                {
                                    //XXX emit error?
                                    AGO_ERROR() << "Motion '" << internalid << "': unable to open recorder";
                                }
                                triggerStart = (int)time(NULL);
                                AGO_DEBUG() << "Motion '" << internalid << "': enable motion trigger and start motion recording";
                                isRecording = true;
    
                                //empty buffer into recorder
                                while( buffer.size()>0 )
                                {
                                    recorder << buffer.front();
                                    buffer.pop();
                                }
                            }
                            catch(cv::Exception& e)
                            {
                                AGO_ERROR() << "Motion '" << internalid << "': opencv exception #4 occured " << e.what();
                            }
                        }

                        //emit security event (enable motion sensor)
                        agoConnection->emitEvent(internalid, "event.device.statechanged", 255, "");
                        isTriggered = true;
                    }
                }
                else
                {
                    //handle recording
                    if( isRecording && now>=(triggerStart+recordDuration) )
                    {
                        AGO_DEBUG() << "Motion '" << internalid << "': stop motion recording";

                        //stop recording
                        if( recorder.isOpened() )
                        {
                            recorder.release();
                        }
                        isRecording = false;

                        //emit video available event
                        Json::Value content;
                        content["filename"] = recordPath.c_str();
                        agoConnection->emitEvent(internalid, "event.device.videoavailable", content);
                    }
                    else
                    {
                        //save current frame
                        recorder << result;
                    }

                    //handle trigger
                    if( isTriggered && now>=(triggerStart+onDuration) )
                    {
                        AGO_DEBUG() << "Motion '" << internalid << "': disable motion trigger";
                        isTriggered = false;

                        //emit security event (disable motion sensor)
                        agoConnection->emitEvent(internalid, "event.device.statechanged", 0, "");
                    }
                }
            }

            //debug purpose: display frame in window
            //waitKey(5);
            
            //check if thread has been interrupted
            boost::this_thread::interruption_point();
        }
    }
    catch(boost::thread_interrupted &e)
    {
        AGO_DEBUG() << "Motion '" << internalid << "': thread interrupted";
    }

    //close all
    provider->unsubscribe(&consumer);
    if( recorder.isOpened() )
    {
        recorder.release();
    }
    //_capture.release();

    AGO_DEBUG() << "Motion '" << internalid << "': stopped";
}

/**
 * Fill motion map
 * @param motion: map to fill
 * @param content: content from request. This map is used to prefill output map
 */
void AgoSurveillance::fillMotion(Json::Value& motion, const Json::Value& content)
{
#if 0
    if( content==NULL )
    {
        //fill with default parameters
        motion["process"] = "";
        motion["name"] = "noname";
        motion["uri"] = "";
        motion["deviation"] = 20;
        motion["sensitivity"] = 10;
        motion["bufferduration"] = 10;
        motion["onduration"] = 300;
        motion["recordduration"] = 30;
        motion["enabled"] = true;
        motion["recordenabled"] = true;
    }
    else
    {
#endif
        //fill with content
        if( !content.isMember("process") )
            motion["process"] = content["process"].asString();
        else
            motion["process"] = "";

        if( !content.isMember("name") )
            motion["name"] = content["name"].asString();
        else
            motion["name"] = "noname";

        if( !content.isMember("uri") )
            motion["uri"] = content["uri"].asString();
        else
            motion["uri"] = "";

        if( !content.isMember("deviation") )
            motion["deviation"] = content["deviation"].asInt();
        else
            motion["deviation"] = 20;

        if( !content.isMember("sensitivity") )
            motion["sensitivity"] = content["sensitivity"].asInt();
        else
            motion["sensitivity"] = 10;

        if( !content.isMember("bufferduration") )
            motion["bufferduration"] = content["bufferduration"].asInt();
        else
            motion["bufferduration"] = 1;

        if( !content.isMember("onduration") )
            motion["onduration"] = content["onduration"].asInt();
        else
            motion["onduration"] = 300;

        if( !content.isMember("recordduration") )
            motion["recordduration"] = content["recordduration"].asInt();
        else
            motion["recordduration"] = 30;

        if( !content.isMember("enabled") )
            motion["enabled"] = content["enabled"].asBool();
        else
            motion["enabled"] = true;

        if( !content.isMember("recordenabled") )
            motion["recordenabled"] = content["recordenabled"].asBool();
        else
            motion["recordenabled"] = true;
#if 0
    }
#endif
}

/**
 * Launch all motions
 */
void AgoSurveillance::launchMotions()
{
    Json::Value& motions = videomap["motions"];
    for( auto it = motions.begin(); it!=motions.end(); it++ )
    {
        std::string internalid = it.name();
        Json::Value& motion = *it;
        launchMotion(internalid, motion);
    }
}

/**
 * Launch specified motion
 */
void AgoSurveillance::launchMotion(std::string internalid, Json::Value& motion)
{
    //create motion device
    agoConnection->addDevice(internalid, "motionsensor");

    AGO_DEBUG() << "Launch motion: " << internalid;
    if( motion["enabled"].asBool()==true )
    {
        boost::thread* thread = new boost::thread(boost::bind(&AgoSurveillance::motionFunction, this, internalid, motion));
        motionThreads[internalid] = thread;
    }
    else
    {
        AGO_DEBUG() << " -> not launch because motion is disabled";
    }
}

/**
 * Stop motion thread
 */
void AgoSurveillance::stopMotion(std::string internalid)
{
    //stop thread
    motionThreads[internalid]->interrupt();
    motionThreads[internalid]->join();

    //remove thread from list
    motionThreads.erase(internalid);
}

/**
 * Get recordings of specified type
 */
void AgoSurveillance::getRecordings(std::string type, Json::Value& list, std::string process)
{
    const Json::Value& motions = videomap["motions"];
    const Json::Value& timelapses = videomap["timelapses"];
    fs::path recordingsPath = getLocalStatePath(RECORDINGSDIR);
    if( fs::exists(recordingsPath) )
    {
        fs::recursive_directory_iterator it(recordingsPath);
        fs::recursive_directory_iterator endit;
        while( it!=endit )
        {
            if( fs::is_regular_file(*it) && it->path().extension().string()==".avi" && boost::algorithm::starts_with(it->path().filename().string(), type) )
            {
                std::vector<std::string> splits;
                split(splits, it->path().filename().string(), boost::is_any_of("_"));
                std::string internalid = std::string(splits[1]);

                bool match = false;

                //check if file is for specified process
                if( process.length()>0 )
                {
                    if( motions.isMember(internalid) )
                    {
                        const Json::Value& motion = motions[internalid];
                        std::string p = motion["process"].asString();
                        if( p==process )
                        {
                            match = true;
                        }
                    }
                    else if( timelapses.isMember(internalid) )
                    {
                        const Json::Value& timelapse = timelapses[internalid];
                        std::string p = timelapse["process"].asString();
                        if( p==process )
                        {
                            match = true;
                        }
                    }
                }
                else
                {
                    // TODO: This does not seem correct?
                    match = true;
                }

                if(match) {
                    Json::Value props;
                    props["filename"] = it->path().filename().string();
                    props["path"] = it->path().string();
                    props["size"] = fs::file_size(it->path());
                    props["date"] =(uint64_t) fs::last_write_time(it->path());
                    props["internalid"] = internalid;
                    list.append( props );
                }
            }
            ++it;
        }
    }
}

/**
 * Return current date and time
 */
std::string AgoSurveillance::getDateTimeString(bool date, bool time, bool withSeparator/*=true*/, std::string fieldSeparator/*="_"*/)
{
    std::stringstream out;
    if( date )
    {
        out << pt::second_clock::local_time().date().year();
        if( withSeparator )
            out << "/";
        int month = (int)pt::second_clock::local_time().date().month();
        if( month<10 )
            out << "0" << month;
        else
            out << month;
        if( withSeparator )
            out << "/";
        int day = (int)pt::second_clock::local_time().date().day();
        if( day<10 )
            out << "0" << day;
        else
            out << day;
    }

    if( date && time )
    {
        out << fieldSeparator;
    }

    if( time )
    {
        int hours = (int)pt::second_clock::local_time().time_of_day().hours();
        if( hours<10 )
            out << "0" << hours;
        else
            out << hours;
        if( withSeparator )
            out << ":";
        int minutes = (int)pt::second_clock::local_time().time_of_day().minutes();
        if( minutes<10 )
            out << "0" << minutes;
        else
            out << minutes;
        if( withSeparator )
            out << ":";
        int seconds = (int)pt::second_clock::local_time().time_of_day().seconds();
        if( seconds<10 )
            out << "0" << seconds;
        else
            out << seconds;
    }

    return out.str();
}

/**
 * Event handler
 */
void AgoSurveillance::eventHandler(const std::string& subject , const Json::Value& content)
{
    if( !videomap.isMember("recordings") )
    {
        //nothing configured, exit right now
        AGO_DEBUG() << "no config";
        return;
    }

    if( subject=="event.environment.timechanged" && content.isMember("minute") && content.isMember("hour") )
    {
        //XXX: workaround to avoid process to crash because of opencv::VideoWriter memory leak
        if( content["hour"].asInt()%restartDelay==0 )
        {
            Json::Value timelapses = videomap["timelapses"];
            if( timelapses.size()>0 )
            {
                signalExit();
            }
        }

        //midnight, create new timelapse for new day
        if( content["hour"].asInt()==0 && content["minute"].asInt()==0 )
        {
            restartTimelapses();
        }
    }
    else if( subject=="event.device.remove" )
    {
        //handle device deletion
        std::string internalid = agoConnection->uuidToInternalId(content["uuid"].asString());
        AGO_DEBUG() << "event.device.remove: " << internalid << " - " << content;
        bool found = false;

        boost::lock_guard<boost::mutex> lock(videomapMutex);

        //search if device is handled by nvr controller
        Json::Value& motions = videomap["motions"];
        if( motions.isMember(internalid) )
        {
            //its a motion device
            found = true;
            motions.removeMember(internalid);
        }
        
        if( !found )
        {
            Json::Value& timelapses = videomap["timelapses"];
            if( timelapses.isMember(internalid) )
            {
                //its a timelapse device
                found = true;
                timelapses.removeMember(internalid);
            }
        }

        if( found )
        {
            if( writeJsonFile(videomap, getConfigPath(VIDEOMAPFILE)) )
            {
                AGO_DEBUG() << "Event 'device.remove': device deleted";
            }
            else
            {
                AGO_ERROR() << "Event 'device.remove': cannot save videomap";
            }
        }
        else
        {
            AGO_DEBUG() << "no device found";
        }
    }
    else if( subject=="event.system.devicenamechanged" )
    {
        //handle device name changed
        bool found = false;
        std::string name = content["name"].asString();
        std::string internalid = agoConnection->uuidToInternalId(content["uuid"].asString());

        boost::lock_guard<boost::mutex> lock(videomapMutex);

        //update motion device
        Json::Value& motions = videomap["motions"];
        if( motions.isMember(internalid) )
        {
            //its a motion device
            found = true;
            Json::Value& motion = motions[internalid];
            if( motion.isMember("name") )
            {
                motion["name"] = name;

                //restart motion
                stopMotion(internalid);
                launchMotion(internalid, motion);
            }
        }
        
        //update timelapse device
        if( !found )
        {
            Json::Value& timelapses = videomap["timelapses"];
            if( timelapses.isMember(internalid) )
            {
                //its a timelapse device
                found = true;
                Json::Value& timelapse = timelapses[internalid];
                if( timelapse.isMember("name") )
                {
                    timelapse["name"] = name;

                    //restart timelapse
                    stopTimelapse(internalid);
                    launchTimelapse(internalid, timelapse);
                }
            }
        }

        if( found )
        {
            if( writeJsonFile(videomap, getConfigPath(VIDEOMAPFILE)) )
            {
                AGO_DEBUG() << "Event 'devicenamechanged': motion name changed";
            }
            else
            {
                AGO_ERROR() << "Event 'devicenamechanged': cannot save videomap";
            }
        }
    }
}

/**
 * Command handler
 */
Json::Value AgoSurveillance::commandHandler(const Json::Value& content)
{
    AGO_DEBUG() << "handling command: " << content;
    Json::Value returnData;
    checkMsgParameter(content, "command", Json::stringValue);

    std::string command = content["command"].asString();
    std::string internalid = content["internalid"].asString();

    if (internalid == "surveillancecontroller")
    {
        if( command=="addstream" )
        {
            checkMsgParameter(content, "process", Json::stringValue);
            checkMsgParameter(content, "uri", Json::stringValue);
           
            //check if stream already exists or not
            boost::lock_guard<boost::mutex> lock(videomapMutex);
            std::string uri = content["uri"].asString();
            Json::Value& streams = videomap["streams"];
            for( auto it = streams.begin(); it!=streams.end(); it++ )
            {
                Json::Value& stream = *it;
                if( stream.isMember("uri") )
                {
                    std::string streamUri = stream["uri"].asString();
                    if( streamUri==uri )
                    {
                        //uri already exists, stop here
                        return responseError("error.security.addstream", "Stream already exists");
                    }
                }
            }

            //fill new stream
            Json::Value stream(Json::objectValue);
            fillStream(stream, content);

            //and save it
            std::string internalid = agocontrol::utils::generateUuid();
            streams[internalid] = stream;
            videomap["streams"] = streams;

            if( writeJsonFile(videomap, getConfigPath(VIDEOMAPFILE)) )
            {
                AGO_DEBUG() << "Command 'addstream': stream added " << stream;

                Json::Value result;
                result["internalid"] = internalid;
                return responseSuccess("Timelapse added", result);
            }
            else
            {
                AGO_ERROR() << "Command 'addstream': cannot save videomap";
                return responseError("error.security.addstream", "Cannot save config");
            }
        }
        else if( command=="addtimelapse" )
        {
            checkMsgParameter(content, "process", Json::stringValue);
            checkMsgParameter(content, "uri", Json::stringValue);
            checkMsgParameter(content, "fps", Json::intValue);
            checkMsgParameter(content, "codec", Json::stringValue);
            checkMsgParameter(content, "enabled", Json::booleanValue);

            //check if timelapse already exists or not
            boost::lock_guard<boost::mutex> lock(videomapMutex);
            std::string uri = content["uri"].asString();
            Json::Value& timelapses = videomap["timelapses"];
            for( auto it = timelapses.begin(); it!=timelapses.end(); it++ )
            {
                const Json::Value& timelapse = *it;
                if( timelapse.isMember("uri") )
                {
                    std::string timelapseUri = timelapse["uri"].asString();
                    if( timelapseUri==uri )
                    {
                        //uri already exists, stop here
                        return responseError("error.security.addtimelapse", "Timelapse already exists");
                    }
                }
            }

            //fill new timelapse
            Json::Value timelapse(Json::objectValue);
            fillTimelapse(timelapse, content);

            //and save it
            std::string internalid = agocontrol::utils::generateUuid();
            timelapses[internalid] = timelapse;
            videomap["timelapses"] = timelapses;
            if( writeJsonFile(videomap, getConfigPath(VIDEOMAPFILE)) )
            {
                AGO_DEBUG() << "Command 'addtimelapse': timelapse added " << timelapse;

                //and finally launch timelapse thread
                launchTimelapse(internalid, timelapse);

                Json::Value result;
                result["internalid"] = internalid;
                return responseSuccess("Timelapse added", result);
            }
            else
            {
                AGO_ERROR() << "Command 'addtimelapse': cannot save videomap";
                return responseError("error.security.addtimelapse", "Cannot save config");
            }
        }
        else if( command=="gettimelapses" )
        {
            checkMsgParameter(content, "process", Json::stringValue);
            std::string process = content["process"].asString();

            Json::Value timelapses(Json::arrayValue);
            getRecordings("timelapse_", timelapses, process);
            returnData["timelapses"].swap(timelapses);
            return responseSuccess(returnData);
        }
        else if( command=="addmotion" )
        {
            checkMsgParameter(content, "process", Json::stringValue);
            checkMsgParameter(content, "uri", Json::stringValue);
            checkMsgParameter(content, "sensitivity", Json::intValue);
            checkMsgParameter(content, "deviation", Json::intValue);
            checkMsgParameter(content, "bufferduration", Json::intValue);
            checkMsgParameter(content, "onduration", Json::intValue);
            checkMsgParameter(content, "recordduration", Json::intValue);
            checkMsgParameter(content, "enabled", Json::booleanValue);

            //check if motion already exists or not
            boost::lock_guard<boost::mutex> lock(videomapMutex);
            std::string uri = content["uri"].asString();
            Json::Value& motions = videomap["motions"];
            for( auto it = motions.begin(); it!=motions.end(); it++ )
            {
                const Json::Value& motion = *it;
                if( motion.isMember("uri") )
                {
                    std::string motionUri = motion["uri"].asString();
                    if( motionUri==uri )
                    {
                        //uri already exists, stop here
                        return responseError("error.security.addmotion", "Motion already exists");
                    }
                }
            }

            // Potentially adjust values
            Json::Value content_(content);
            if( content_["recordduration"].asInt() >= content_["onduration"].asInt() )
            {
                AGO_WARNING() << "Addmotion: record duration must be lower than on duration. Record duration forced to on duration.";
                content_["recordduration"] = content_["onduration"].asInt() - 1;
            }
            if( content_["bufferduration"].asInt() >= content_["recordduration"].asInt() )
            {
                AGO_WARNING() << "Addmotion: buffer duration must be lower than record duration. Buffer duration forced to record duration.";
                content_["bufferduration"] = content_["recordduration"].asInt() - 1;
            }

            //fill new motion
            Json::Value motion(Json::objectValue);
            fillMotion(motion, content_);

            //and save it
            std::string internalid = agocontrol::utils::generateUuid();
            motions[internalid] = motion;
            if( writeJsonFile(videomap, getConfigPath(VIDEOMAPFILE)) )
            {
                AGO_DEBUG() << "Command 'addmotion': motion added " << motion;

                //and finally launch motion thread
                launchMotion(internalid, motion);

                Json::Value result;
                result["internalid"] = internalid;
                return responseSuccess("Motion added", result);
            }
            else
            {
                AGO_ERROR() << "Command 'addmotion': cannot save videomap";
                return responseError("error.security.addmotion", "Cannot save config");
            }
        }
        else if( command=="getmotions" )
        {
            checkMsgParameter(content, "process", Json::stringValue);
            std::string process = content["process"].asString();

            Json::Value motions(Json::arrayValue);
            getRecordings("motion_", motions, process);
            returnData["motions"].swap(motions);
            return responseSuccess(returnData);
        }
        else if( command=="getrecordingsconfig" )
        {
            Json::Value config = videomap["recordings"];
            returnData["recordings"].swap(config);
            return responseSuccess(returnData);
        }
        else if( command=="setrecordingsconfig" )
        {
            checkMsgParameter(content, "timelapseslifetime", Json::intValue);
            checkMsgParameter(content, "motionslifetime", Json::intValue);

            boost::lock_guard<boost::mutex> lock(videomapMutex);
            Json::Value config = videomap["recordings"];
            config["timelapseslifetime"] = content["timelapseslifetime"].asInt();
            config["motionslifetime"] = content["motionslifetime"].asInt();

            if( writeJsonFile(videomap, getConfigPath(VIDEOMAPFILE)) )
            {
                AGO_DEBUG() << "Command 'setrecordingsconfig': recordings config stored";
                return responseSuccess();
            }
            else
            {
                AGO_ERROR() << "Command 'setrecordingsconfig': cannot save videomap";
                return responseError("error.security.setrecordingsconfig", "Cannot save config");
            }
        }

        return responseUnknownCommand();
    }
    else
    {
        //device commands
        
        if( command=="getvideoframe")
        {
            //handle get video frame of camera device (internal stream object)
            std::string internalid = agoConnection->uuidToInternalId(content["uuid"].asString());

            //get stream infos
            Json::Value stream;
            std::string uri = "";
            {
                boost::lock_guard<boost::mutex> lock(videomapMutex);
                Json::Value& streams = videomap["streams"];
                if( streams.isMember(internalid) )
                {
                    stream = streams[internalid];
                    uri = stream["uri"].asString();
                }
            }

            if( uri.length()>0 )
            {
                //get or create provider
                AgoFrameProvider* provider = getFrameProvider(uri);
                if( provider==NULL )
                {
                    //no frame provider
                    AGO_ERROR() << "Command 'getvideoframe': unable to get frame provider for stream '" << uri << "'";
                    return responseError("error.nvr.getvideoframe.", "Unable to get frame provider for specified stream");
                }

                //subscribe consumer to frame provider
                AgoFrameConsumer consumer;
                provider->subscribe(&consumer);
                
                //get frame
                bool frameReceived = false;
                Mat frame;
                while( !frameReceived )
                {
                    // XXX: Blocking agocontrol main loop?
                    if( provider->isRunning() )
                    {
                        frame = consumer.popFrame(&boost::this_thread::sleep_for);
                        frameReceived = true;
                    }
                }

                //unsubscribe
                provider->unsubscribe(&consumer);

                //return image under base64 format
                std::string img;
                if( frameToString(frame, img) )
                {
                    Json::Value result;
                    result["image"] = base64_encode(reinterpret_cast<const unsigned char*>(img.c_str()), img.length());
                    return responseSuccess(result);
                }
                else
                {
                    AGO_ERROR() << "Command: 'getvideoframe': unable to convert image to string";
                    return responseError("error.nvr.getvideoframe.", "Unable to get image from stream");
                }
            }
            else
            {
                return responseError("error.nvr.getvideoframe.", "Unable to get frame for empty stream uri");
            }
        }
        else if( command=="on" || command=="off" )
        {
            //handle on/off command on timelapse/motion devices
            bool found = false;
            std::string internalid = agoConnection->uuidToInternalId(content["uuid"].asString());

            boost::lock_guard<boost::mutex> lock(videomapMutex);

            //search timelapse
            Json::Value& timelapses = videomap["timelapses"];
            if( timelapses.isMember(internalid) )
            {
                found = true;
                Json::Value& timelapse = timelapses[internalid];
                if( timelapse.isMember("enabled") )
                {
                    if( command=="on" )
                    {
                        timelapse["enabled"] = true;

                        //start timelapse
                        launchTimelapse(internalid, timelapse);
                    }
                    else
                    {
                        timelapse["enabled"] = false;

                        //start timelapse
                        stopTimelapse(internalid);
                    }
                }
            }

            if( !found )
            {
                //search motion
                Json::Value& motions = videomap["motions"];
                if( motions.isMember(internalid) )
                {
                    found = true;
                    Json::Value& motion = motions[internalid];
                    if( motion.isMember("enabled") )
                    {
                        if( command=="on" )
                        {
                            motion["enabled"] = true;

                            //start motion
                            launchMotion(internalid, motion);
                        }
                        else
                        {
                            motion["enabled"] = false;

                            //stop motion
                            stopMotion(internalid);
                        }
                    }
                }
            }

            if( found )
            {
                if( writeJsonFile(videomap, getConfigPath(VIDEOMAPFILE)) )
                {
                    AGO_DEBUG() << "Command " << command << ": timelapse " << internalid << " started";
                }
                else
                {
                    AGO_ERROR() << "Command " << command << ": cannot save videomap";
                }
            }
        }
        else if( command=="recordon" || command=="recordoff" )
        {
            //handle record on/off command on motion devices
            bool found = false;
            std::string internalid = agoConnection->uuidToInternalId(content["uuid"].asString());

            boost::lock_guard<boost::mutex> lock(videomapMutex);

            Json::Value& motions = videomap["motions"];
            if( motions.isMember(internalid) )
            {
                found = true;
                Json::Value& motion = motions[internalid];
                if( motion.isMember("enabled") )
                {
                    if( command=="recordon" )
                    {
                        motion["recordenabled"] = true;
                    }
                    else
                    {
                        motion["recordenabled"] = false;
                    }

                    //restart motion
                    stopMotion(internalid);
                    launchMotion(internalid, motion);
                }
            }

            if( found )
            {
                if( writeJsonFile(videomap, getConfigPath(VIDEOMAPFILE)) )
                {
                    AGO_DEBUG() << "Command " << command << ": timelapse " << internalid << " started";
                }
                else
                {
                    AGO_ERROR() << "Command " << command << ": cannot save videomap";
                }
            }
        }

        return responseUnknownCommand();
    }

    // We have no devices registered but our own
    throw std::logic_error("Should not go here");
}

void AgoSurveillance::setupApp()
{
    //config
    std::string optString = getConfigOption("restartDelay", "12");
    sscanf(optString.c_str(), "%d", &restartDelay);

    //load config
    if(!readJsonFile(videomap, getConfigPath(VIDEOMAPFILE)))
        videomap = Json::Value(Json::objectValue);

    bool inited = false;
    //add missing sections if necessary
    if( !videomap.isMember("streams") )
    {
        videomap["streams"] = Json::Value(Json::objectValue);
        inited = true;
    }
    if( !videomap.isMember("timelapses") )
    {
        videomap["timelapses"] = Json::Value(Json::objectValue);
        inited = true;
    }
    if( !videomap.isMember("motions") )
    {
        videomap["motions"] = Json::Value(Json::objectValue);
        inited = true;
    }
    if( !videomap.isMember("recordings") )
    {
        Json::Value recordings;
        recordings["timelapseslifetime"] = 7;
        recordings["motionslifetime"] = 14;
        videomap["recordings"] = recordings;
        inited = true;
    }

    AGO_DEBUG() << "Loaded videomap: " << videomap;

    if(inited)
        writeJsonFile(videomap, getConfigPath(VIDEOMAPFILE));

    //finalize
    agoConnection->addDevice("surveillancecontroller", "surveillancecontroller");
    addCommandHandler();
    addEventHandler();

    //launch timelapse threads
    launchTimelapses();

    //launch motion threads
    launchMotions();
}

void AgoSurveillance::cleanupApp()
{
    //stop processes
    stopProcess = true;

    //wait for timelapse threads stop
    for( std::map<std::string, boost::thread*>::iterator it=timelapseThreads.begin(); it!=timelapseThreads.end(); it++ )
    {
        stopTimelapse(it->first);
    }

    //wait for motion threads stop
    for( std::map<std::string, boost::thread*>::iterator it=motionThreads.begin(); it!=motionThreads.end(); it++ )
    {
        stopMotion(it->first);
    }

    //close frame providers
    for( std::map<std::string, AgoFrameProvider*>::iterator it=frameProviders.begin(); it!=frameProviders.end(); it++ )
    {
        (it->second)->stop();
    }
}

AGOAPP_ENTRY_POINT(AgoSurveillance);

