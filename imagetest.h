#ifndef IMAGETEST_H
#define IMAGETEST_H

#include "worker.h"
#include <QWidget>
#include <QCamera>
#include <QCameraViewfinder>
#include <QCameraImageCapture>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QSslConfiguration>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <QBuffer>
#include <QByteArray>
#include <QJsonArray>
#include <QThread>
#include <QPainter>
#include <QCameraInfo>
#include <QComboBox>

QT_BEGIN_NAMESPACE
namespace Ui { class ImageTest; }
QT_END_NAMESPACE

class ImageTest : public QWidget
{
    Q_OBJECT

signals: void beginWork(QImage img,QThread *childThread );

public slots:
    void showCamera(int id, QImage preview);
    void takePicture();
    void tokenReply(QNetworkReply *reply);
    void imgReply(QNetworkReply *reply);
    void prePostData();
    void beginFaceDetect(QByteArray postData,QThread *overThread);
    void pickCamera(int idx);
public:
    ImageTest(QWidget *parent = nullptr);
    ~ImageTest();

private:
    Ui::ImageTest *ui;
    QCamera *camera;
    QCameraImageCapture *imageCapture;
    QCameraViewfinder *finder;
    QTimer *refreshTimer;
    QTimer *netTimer;
    QNetworkAccessManager *tokenManager;
    QNetworkAccessManager *imgManager;
    QSslConfiguration sslConfig;
    QString accessToken;
    QImage img;
    QThread *childThread;

    QList<QCameraInfo> cameraInfoList;

    double faceleft;
    double facewidth;
    double facetop;
    double faceheight;

    double age;
    QString gender;
    int mask;
    double beauty;

    int latestTime;
};
#endif // IMAGETEST_H
