#include <QApplication>
#include <QCommandLineParser>
#include <QLocale>
#include <QTranslator>
#include <QLockFile>
#include <QMessageBox>
#include <QProcess>
#include <QSplashScreen>
#include <QStyleFactory>
#include <QSettings>

#include "CommandLineParser.h"
#include "CurrencyAdapter.h"
#include "LoggerAdapter.h"
#include "NodeAdapter.h"
#include "Settings.h"
#include "SignalHandler.h"
#include "WalletAdapter.h"
#include "gui/MainWindow.h"
#include "Update.h"
#include <QTextCodec>
#include "PaymentServer.h"
#include "contrib.hpp"

#define DEBUG 1

using namespace WalletGui;

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  app.setApplicationName(CurrencyAdapter::instance().getCurrencyName() + "wallet");
  app.setApplicationVersion(Settings::instance().getVersion());
  app.setQuitOnLastWindowClosed(false);

#ifndef Q_OS_MAC
  QApplication::setStyle(QStyleFactory::create("Fusion"));
#endif

  CommandLineParser cmdLineParser(nullptr);
  Settings::instance().setCommandLineParser(&cmdLineParser);
  bool cmdLineParseResult = cmdLineParser.process(app.arguments());
  Settings::instance().load();
  QTranslator translator;
  QTranslator translatorQt;

  QString lng = Settings::instance().getLanguage();

  if(!lng.isEmpty()) {
      translator.load(":/languages/" + lng + ".qm");
      translatorQt.load(":/languages/qt_" + lng + ".qm");

      if(lng == "uk") {
            QLocale::setDefault(QLocale("uk_UA"));
        } else if(lng == "ru") {
            QLocale::setDefault(QLocale("ru_RU"));
        } else if(lng == "pl") {
            QLocale::setDefault(QLocale("pl_PL"));
        } else if(lng == "be") {
            QLocale::setDefault(QLocale("be_BY"));
        } else if(lng == "de") {
            QLocale::setDefault(QLocale("de_DE"));
        } else if(lng == "es") {
            QLocale::setDefault(QLocale("es_ES"));
        } else if(lng == "fr") {
            QLocale::setDefault(QLocale("fr_FR"));
        } else if(lng == "pt") {
            QLocale::setDefault(QLocale("pt_BR"));
        } else {
            QLocale::setDefault(QLocale::c());
        }

    } else {
      translator.load(":/languages/" + QLocale::system().name());
      translatorQt.load(":/languages/qt_" +  QLocale::system().name());
      QLocale::setDefault(QLocale::system().name());
  }
  app.installTranslator(&translator);
  app.installTranslator(&translatorQt);

  //QLocale::setDefault(QLocale::c());

  //QLocale locale = QLocale("uk_UA");
  //QLocale::setDefault(locale);

  setlocale(LC_ALL, "");

  QFile File(":/skin/default.qss");
  File.open(QFile::ReadOnly);
  QString StyleSheet = QLatin1String(File.readAll());
  qApp->setStyleSheet(StyleSheet);

  if (PaymentServer::ipcSendCommandLine())
  exit(0);

  PaymentServer* paymentServer = new PaymentServer(&app);

#ifdef Q_OS_WIN
  if(!cmdLineParseResult) {
    QMessageBox::critical(nullptr, QObject::tr("Error"), cmdLineParser.getErrorText());
    return app.exec();
  } else if (cmdLineParser.hasHelpOption()) {
    QMessageBox::information(nullptr, QObject::tr("Help"), cmdLineParser.getHelpText());
    return app.exec();
  }

  //Create registry entries for URL execution
  QSettings parsicoinKey("HKEY_CLASSES_ROOT\\Catalyst", QSettings::NativeFormat);
  parsicoinKey.setValue(".", "Catalyst Wallet");
  parsicoinKey.setValue("URL Protocol", "");
  QSettings parsicoinOpenKey("HKEY_CLASSES_ROOT\\catalyst\\shell\\open\\command", QSettings::NativeFormat);
  parsicoinOpenKey.setValue(".", "\"" + QCoreApplication::applicationFilePath().replace("/", "\\") + "\" \"%1\"");
#endif

#if defined(Q_OS_LINUX)
  QStringList args;
  QProcess exec;

  //as root
  args << "-c" << "printf '[Desktop Entry]\\nName = XAT URL Handler\\nGenericName = catalyst\\nComment = Handle URL Sheme catalyst://\\nExec = " + QCoreApplication::applicationFilePath() + " %%u\\nTerminal = false\\nType = Application\\nMimeType = x-scheme-handler/catalyst;\\nIcon = Catalyst-Wallet' | tee /usr/share/applications/Catalyst-handler.desktop";
  exec.start("/bin/sh", args);
  exec.waitForFinished();

  args.clear();
  args << "-c" << "update-desktop-database";
  exec.start("/bin/sh", args);
  exec.waitForFinished();
#endif

  LoggerAdapter::instance().init();

  QString dataDirPath = Settings::instance().getDataDir().absolutePath();

  if (!QDir().exists(dataDirPath)) {
    QDir().mkpath(dataDirPath);
  }

  QLockFile lockFile(Settings::instance().getDataDir().absoluteFilePath(QApplication::applicationName() + ".lock"));
  if (!lockFile.tryLock()) {
    QMessageBox::warning(nullptr, QObject::tr("Fail"), QObject::tr("%1 wallet already running or cannot create lock file %2. Check your permissions.").arg(CurrencyAdapter::instance().getCurrencyDisplayName()).arg(Settings::instance().getDataDir().absoluteFilePath(QApplication::applicationName() + ".lock")));
    return 0;
  }

  SignalHandler::instance().init();
  QObject::connect(&SignalHandler::instance(), &SignalHandler::quitSignal, &app, &QApplication::quit);

  QSplashScreen* splash = new QSplashScreen(QPixmap(":images/splash"), /*Qt::WindowStaysOnTopHint |*/ Qt::X11BypassWindowManagerHint);
  if (!splash->isVisible()) {
    splash->show();
  }

  splash->showMessage(QObject::tr("Loading blockchain..."), Qt::AlignLeft | Qt::AlignBottom, Qt::white);

  app.processEvents();
  qRegisterMetaType<CryptoNote::TransactionId>("CryptoNote::TransactionId");
  qRegisterMetaType<quintptr>("quintptr");
  if (!NodeAdapter::instance().init()) {
    return 0;
  }

  splash->finish(&MainWindow::instance());

  Updater d;
    d.checkForUpdate();
  MainWindow::instance().show();
  WalletAdapter::instance().open("");

  QTimer::singleShot(1000, paymentServer, SLOT(uiReady()));
  QObject::connect(paymentServer, &PaymentServer::receivedURI, &MainWindow::instance(), &MainWindow::handlePaymentRequest, Qt::QueuedConnection);

  QObject::connect(QApplication::instance(), &QApplication::aboutToQuit, []() {
    MainWindow::instance().quit();
    if (WalletAdapter::instance().isOpen()) {
      WalletAdapter::instance().close();
    }

    NodeAdapter::instance().deinit();
  });

  return app.exec();
}
