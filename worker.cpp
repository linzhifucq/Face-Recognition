#include "worker.h"

Worker::Worker(QObject *parent)
    : QObject{parent}
{

}

//转成base64编码    (进行base64编码的转化时会卡顿，是因为程序在处理转换编码时又接收到刷新页面的请求，
//  但是转换编码没有完成，刷新页面的请求被排在后面，页面没有进行刷新，所以出现卡顿现象)
//所以这里要使用多线程。。开启一个主线程和子线程，主线程（ui线程）执行以后，开启事件循环（eventloop），等待事件发生（信号到来），信号到来后去调用对应的槽函数。
//而子线程就用来处理一些耗时长的操作，比如这里可以用子线程处理base64转换这个事件。
void Worker::doWork(QImage img,QThread *childThread) {
    QByteArray ba;  //设置字节数组
    QBuffer buff(&ba);  //让字节数组像文件一样可以进行io操作
    img.save(&buff,"png");  //把图片按png格式保存到buff中
    QString b64str = ba.toBase64();  //转成对应的base64编码
    //qDebug()<<b64str;

    //请求体body参数设置
    QJsonObject postJson;
    QJsonDocument doc;

    postJson.insert("image",b64str);
    postJson.insert("image_type","BASE64");
    postJson.insert("face_field","age,expression,face_shape,gender,glasses,emotion,face_type,mask,beauty");
    doc.setObject(postJson);
    QByteArray postData = doc.toJson(QJsonDocument::Compact);

    emit resultReady(postData,childThread);
}
