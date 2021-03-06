#ifndef FRAMEPROVIDER_H
#define FRAMEPROVIDER_H

#include <string>
#include <queue>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include <boost/chrono/chrono.hpp>
#include <boost/chrono/duration.hpp>

using namespace cv;
using namespace boost;

typedef void (*CallbackBoostThreadSleep)(const boost::chrono::duration<int, boost::milli>&);

/*class AgoFrame
{
    private:
        pthread_mutex_t _mutex;
        int _consumers;

    public:
        AgoFrame(int _consumers);
        ~AgoFrame();

        Mat frame;
        void done();
        int getConsumers();
};*/

class AgoFrameConsumer
{
    private:
        std::string _id;
        std::queue<Mat> _frames;

    public:
        AgoFrameConsumer();
        ~AgoFrameConsumer();

        std::string getId();
        //void pushFrame(AgoFrame* frame);
        void pushFrame(Mat frame);
        //AgoFrame* popFrame(CallbackBoostThreadSleep sleepCallback);
        Mat popFrame(CallbackBoostThreadSleep sleepCallback);
};

class AgoFrameProvider
{
    private:
        std::string _uri;
        //queue<AgoFrame*> _frames;
        std::queue<Mat> _frames;
        std::list<AgoFrameConsumer*> _consumers;
        boost::thread* _thread;
        int _fps;
        Size _resolution;
        bool _isRunning;
        VideoCapture* _capture;

        void threadFunction();

    public:
        AgoFrameProvider(std::string uri);
        ~AgoFrameProvider();
    
        bool start();
        void stop();
        void subscribe(AgoFrameConsumer* consumer);
        void unsubscribe(AgoFrameConsumer* consumer);
        int getFps();
        Size getResolution();
        bool isRunning();
};

#endif

