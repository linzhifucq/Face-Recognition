#include "imagetest.h"
#include "ui_imagetest.h"

ImageTest::ImageTest(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ImageTest)
{
    ui->setupUi(this);
    cameraInfoList = QCameraInfo::availableCameras();
    for (const QCameraInfo &tmpCam  : cameraInfoList){
     //    qDebug() <<tmpCam.deviceName()<<"|||"<<tmpCam.description();
        ui->comboBox->addItem(tmpCam.description());
    }

    connect(ui->comboBox,QOverload<int>::of(&QComboBox::currentIndexChanged),this,&ImageTest::pickCamera);
    //设置摄像头的功能
    camera = new QCamera();
    finder = new QCameraViewfinder();
    imageCapture = new QCameraImageCapture(camera);
    camera->setViewfinder(finder);
    camera->setCaptureMode(QCamera::CaptureStillImage);
    imageCapture->setCaptureDestination(QCameraImageCapture::CaptureToBuffer);

    connect(imageCapture,&QCameraImageCapture::imageCaptured,this,&ImageTest::showCamera);
    connect(ui->pushButton,&QPushButton::clicked,this,&ImageTest::prePostData);
    camera->start();
    //进行界面布局
    this->resize(1200,700);
    QVBoxLayout *vboxl = new QVBoxLayout;
    vboxl->addWidget(ui->label);
    vboxl->addWidget(ui->pushButton);

    QVBoxLayout *vboxr = new QVBoxLayout;
    vboxr->addWidget(ui->comboBox);
    vboxr->addWidget(finder);
    vboxr->addWidget(ui->textBrowser);

    QHBoxLayout *hbox = new QHBoxLayout(this);
    hbox->addLayout(vboxl);
    hbox->addLayout(vboxr);
    this->setLayout(hbox);

    //利用定时器不断刷新页面
    refreshTimer = new QTimer(this);
    connect(refreshTimer,&QTimer::timeout,this,&ImageTest::takePicture);
    refreshTimer->start(20);

    //利用定时器不断进行人脸识别请求
    netTimer = new QTimer(this);
    connect(netTimer,&QTimer::timeout,this,&ImageTest::prePostData);

    tokenManager = new QNetworkAccessManager(this);
    connect(tokenManager,&QNetworkAccessManager::finished,this,&ImageTest::tokenReply);

    imgManager = new QNetworkAccessManager(this);
    connect(imgManager,&QNetworkAccessManager::finished,this,&ImageTest::imgReply);

    qDebug()<<tokenManager->supportedSchemes();

    //拼接url及参数
    QUrl url("https://aip.baidubce.com/oauth/2.0/token");


    QUrlQuery query;
    query.addQueryItem("grant_type","client_credentials");
    query.addQueryItem("client_id","7bNIbpYFao8Y1VaHbMkCFfDz");
    query.addQueryItem("client_secret","X4QAWXzeNVai68pOnRG2G7cjEcnKxsLl");

    url.setQuery(query);
    qDebug()<<url;

    //ssl支持
    if(QSslSocket::supportsSsl()) {
        qDebug()<< "支持";
    }
    else {
        qDebug()<< "不支持";
    }

    //进行ssl配置

    //获取默认配置
    sslConfig =  QSslConfiguration::defaultConfiguration();
    //设置验证模式
    sslConfig.setPeerVerifyMode(QSslSocket::QueryPeer);
    //设置协议
    sslConfig.setProtocol(QSsl::TlsV1_2);

    //组装请求
    QNetworkRequest req;
    req.setUrl(url);
    req.setSslConfiguration(sslConfig);

    //发送请求
    tokenManager->get(req);



}

void ImageTest::showCamera(int id, QImage img) {
    Q_UNUSED(id);
    this->img = img;

    //绘制人脸框
    QPainter p(&img);
    p.setPen(Qt::red);
    p.drawRect(faceleft,facetop,facewidth,faceheight);
    //设置字体
    QFont font = p.font();
    font.setPixelSize(30);
    p.setFont(font);

    p.drawText(faceleft+facewidth+5,facetop,QString("年龄:").append(QString::number(age)));
    p.drawText(faceleft+facewidth+5,facetop+40,QString("性别:").append(gender));
    p.drawText(faceleft+facewidth+5,facetop+80,QString("戴口罩:").append(mask==0?"没带口罩":"戴口罩了"));
    p.drawText(faceleft+facewidth+5,facetop+120,QString("颜值:").append(QString::number(beauty)));
    ui->label->setPixmap(QPixmap::fromImage(img));
}

void ImageTest::takePicture() {
    imageCapture->capture();
}

void ImageTest::tokenReply(QNetworkReply *reply) {
    //错误处理
    if(reply->error()!=QNetworkReply::NoError) {
        qDebug()<<reply->errorString();
        return;
    }

    //正常应答
    const QByteArray reply_data = reply->readAll();
    //qDebug()<<reply_data;

    //json解析
    QJsonParseError jsonErr;
    QJsonDocument doc =  QJsonDocument::fromJson(reply_data,&jsonErr);

    //解析成功
    if(jsonErr.error==QJsonParseError::NoError) {
        QJsonObject obj = doc.object();
        if(obj.contains("access_token")) {
            accessToken = obj.take("access_token").toString();
        }
        ui->textBrowser->setText(accessToken);
    }
    else {
        qDebug()<<"JSON ERR:"<<jsonErr.errorString();
    }

    reply->deleteLater();
    netTimer->start(1500);

}

void ImageTest::prePostData() {
    //创建子线程，创建worker，把worker送进子线程，然后绑定信号和槽函数启动子线程后，发通知让worker开始工作
    //worker完成工作后返回postData数据
    childThread = new QThread(this);
    Worker *worker = new Worker;
    worker->moveToThread(childThread);
    //通知开始干活
    connect(this,&ImageTest::beginWork,worker,&Worker::doWork);
    //干完活后通知主线程
    connect(worker,&Worker::resultReady,this,&ImageTest::beginFaceDetect);
    //子线程结束后调用deleteLater来销毁分配的内存，防止内存泄露
    connect(childThread, &QThread::finished, worker, &QObject::deleteLater);

    childThread->start();

    emit beginWork(img,childThread);

}
//可以利用多线程，线程池知识（比如说我写了4个子线程，然后子线程堵塞在这里等待我任务队列的封装好的任务，就是封装好的任务包，这个任务包就是待会我们要分析，编码的图片。
//子线程接收任务然后工作，然后子线程把任务干完了就会来任务队列取任务，同时我们可以给任务队列上个锁，就是上个线程锁，任何人对队列进行操作时必须加锁操作，保证
//队列不会乱套，然后这里有一个问题就是只有4个子线程，我子线程解决问题的速度可能赶不上任务包进任务队列的速度，这样的话任务也会堆积在这里。这里的话因为有4个子线程
//，所以我们这里任务队列可以就保存4个，当有第5个任务包进来后，那第1个就可以被踢出队列。  我们就可以利用这样的一个思路去维护我们的任务队列，让4个子线程饱和地进行
//任务工作，这样就可以使得我们的人脸刷新频率能达到计算机比较等承受的程度，让计算机资源得到一个合理的利用，然后也不至于崩溃。）
void ImageTest::beginFaceDetect(QByteArray postData,QThread *overThread) {
    //关闭子线程(发信号总结子线程，总结的结果不会立刻返回，它是异步的，这时查看子线程状态发现还在运行，
    //虽然子线程过了一下马上关闭了，但是某个时刻它是双线程在跑的，如果开了多个线程还是会有危险的 )
    //调用QThread的wait方法

    overThread->exit();
    overThread->wait();

    if(childThread->isFinished()) {
        qDebug()<<"子线程结束了";
    } else
    {
        qDebug()<<"子线程没有结束";
    }


    //组装图像识别请求
    QUrl url("https://aip.baidubce.com/rest/2.0/face/v3/detect");

    QUrlQuery query;
    query.addQueryItem("access_token",accessToken);
    url.setQuery(query);

    //组装请求
    QNetworkRequest req;
    req.setHeader(QNetworkRequest::ContentTypeHeader,QVariant("application/json"));
    req.setUrl(url);
    req.setSslConfiguration(sslConfig);

    imgManager->post(req,postData);
}

void ImageTest::imgReply(QNetworkReply *reply) {
    //错误处理
    if(reply->error() != QNetworkReply::NoError ) {
        qDebug()<<reply->errorString();
        return;
    }
    //正常应答
    const QByteArray replyData = reply->readAll();
    //qDebug()<<replyData;
    //json解析
    QJsonParseError jsonErr;
    QJsonDocument doc =  QJsonDocument::fromJson(replyData,&jsonErr);

    //解析成功
    if(jsonErr.error==QJsonParseError::NoError) {
        QString faceInfo;

        //取出最外层的json
        QJsonObject obj = doc.object();

        //判断时间
        if(obj.contains("timestamp")) {
            int tmpTime = obj.take("timestamp").toInt();
            if(tmpTime < latestTime) {
                return;
            }
            else {
                latestTime = tmpTime;
            }
        }

        if(obj.contains("result")) {
            QJsonObject resultObj = obj.take("result").toObject();

            //取出人脸列表
            if(resultObj.contains("face_list")) {
                QJsonArray faceList = resultObj.take("face_list").toArray();
                //取出第一张人脸信息
                QJsonObject faceObject = faceList.at(0).toObject();

                //取出人脸定位信息
                if(faceObject.contains("location")) {
                    QJsonObject locObj = faceObject.take("location").toObject();
                    if(locObj.contains("left")) {
                       faceleft = locObj.take("left").toDouble();
                    }
                    if(locObj.contains("width")) {
                        facewidth = locObj.take("width").toDouble();
                    }
                    if(locObj.contains("top")) {
                        facetop = locObj.take("top").toDouble();
                    }
                    if(locObj.contains("height")) {
                        faceheight = locObj.take("height").toDouble();
                    }

                }

                //取出年龄
                if(faceObject.contains("age")) {
                    age = faceObject.take("age").toDouble();
                    faceInfo.append("年龄:").append(QString::number(age)).append("\r\n");
                }

                //取出性别
                if(faceObject.contains("gender")) {
                    QJsonObject genderObj = faceObject.take("gender").toObject();
                    if(genderObj.contains("type")) {
                        gender = genderObj.take("type").toString();
                        faceInfo.append("性别:").append(gender).append("\r\n");
                    }
                }

                //取出表情
                if(faceObject.contains("emotion")) {
                    QJsonObject emotionObj = faceObject.take("emotion").toObject();
                    if(emotionObj.contains("type")) {
                        QString emotion = emotionObj.take("type").toString();
                        faceInfo.append("表情:").append(emotion).append("\r\n");
                    }
                }

                //取出是否戴面具
                if(faceObject.contains("mask")) {
                    QJsonObject maskObj = faceObject.take("mask").toObject();
                    if(maskObj.contains("type")) {
                        mask = maskObj.take("type").toInt();
                        faceInfo.append("面具:").append(mask == 0?"没带口罩":"戴口罩了").append("\r\n");
                    }
                }

                //取出颜值
                if(faceObject.contains("beauty")) {
                    beauty = faceObject.take("beauty").toDouble();
                    faceInfo.append("颜值:").append(QString::number(beauty)).append("\r\n");
                }

            }
        }
        ui->textBrowser->setText(faceInfo);
    }
    else {
        qDebug()<<"JSON ERR:"<<jsonErr.errorString();
    }

}

ImageTest::~ImageTest()
{
    delete ui;
}

void ImageTest::pickCamera(int idx) {
    //输出摄像头列表的信息
    qDebug()<<cameraInfoList.at(idx).description();
    //停止摄像头定时器的刷新
    refreshTimer->stop();
    //关闭摄像头
    camera->stop();
    //新建摄像头
    camera = new QCamera(cameraInfoList.at(idx));
    imageCapture = new QCameraImageCapture(camera);

    connect(imageCapture,&QCameraImageCapture::imageCaptured,this,&ImageTest::showCamera);
    imageCapture->setCaptureDestination(QCameraImageCapture::CaptureToBuffer);
    camera->setCaptureMode(QCamera::CaptureStillImage);
    camera->setViewfinder(finder);

    camera->start();
    refreshTimer->start(10);
}

