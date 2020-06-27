#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QDebug>
#include <QQmlContext>
#include <QStandardPaths>

#include <QProgressDialog>
#include <QApplication>

#include "mpv/mpv.h"
#include "library/schema.h"
#include "library/models.h"
#include "library/importer.h"
#include "logic/app.h"
#include "settings.h"

#include <iostream>


using namespace Midoku::Library;

int main(int argc, char *argv[]) {
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setOrganizationDomain("oro.sodimm.me");
    QCoreApplication::setApplicationName("Midoku");
    QCoreApplication::setApplicationVersion("0.1");

    QApplication qapp(argc, argv);

    std::setlocale(LC_NUMERIC, "C");

    qRegisterMetaType<Book*>("Library::Book*");
    qRegisterMetaType<Chapter*>("Library::Chapter*");

    qmlRegisterType<Mpv>("midoku.mpv", 1, 0, "Mpv");
    qmlRegisterAnonymousType<Book>("midoku.mpv", 1);
    qmlRegisterAnonymousType<Chapter>("midoku.mpv", 1);
    qmlRegisterAnonymousType<Progress>("midoku.mpv", 1);
    qmlRegisterAnonymousType<TableModel>("midoku.mpv", 1);
    qmlRegisterAnonymousType<Midoku::App>("midoku.mpv", 1);
    qmlRegisterAnonymousType<Midoku::Player>("midoku.mpv", 1);

    Database db("test.db");
    {
        auto res = upgrade_schema(&db);
        if (!res) {
            std::cerr << res.error().text().toUtf8().constData() << std::endl;

            std::fflush(stdout);
            std::fflush(stderr);
            return 221;
        }
    }

    {
        auto q = db.prepare("PRAGMA user_version;");
        if (!q.exec())
            qDebug() << q.lastError();

        q.next();
        qDebug() << "Schema Version:" << q.value(0).toInt();
    }

    {
        Importer i (db);
        QProgressDialog dlg;
        i.connect(&i, &Importer::progress, [&dlg, &qapp](int cur, int max, QString msg) {
            dlg.setMaximum(max);
            dlg.setValue(cur);
            dlg.setLabelText(msg);
            qapp.processEvents();
        });
        dlg.show();
        qapp.processEvents();

        Midoku::Settings s;
        QStringList paths = s.getLibraryPaths();
        if (paths.isEmpty()) {
            paths.append(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/Audiobooks");
            s.setLibraryPaths(paths);
        }

        auto r = i.import(s.getLibraryPaths2());
        if (!r)
            qDebug() << r.error();
        dlg.hide();
    }

#if 0
    {
        Midoku::Library::Book b(db, "Dragons at Dorcastle", "Jack Campbell", "MacLeod Andrews");
        b.connect(&b, b.changedSignal(b.id), [] () {qDebug() << "ID Changed!!!"; });
        auto r = b.save();
        if (r)
            qDebug() << "Saved as:" << r.value();
        else
            qDebug() << "Save error:" << r.error();
    }

    {
        auto r = db.select<Book>(Book::id == 1)
                .map([] (auto bs) {
            if (bs.size() < 1)
                return;
            auto b = std::move(bs[0]);
        //auto r = db.load<Midoku::Library::Book>(1)
        //        .map([](auto b) {
            qDebug() << "Title:" << b->get(b->title);

            /*b->set(b->reader, b->get(b->reader).append("_1"));
            auto r = b->save();
            if (!r)
                qDebug() << "Re-Save Error:" << r.error();*/

            qDebug() << "Prop:" << b->property("title");

            auto t = b->getChapters().map([] (auto cs) {
                qDebug() << cs[0]->get(Chapter::id) << cs[0]->get(Chapter::media);
                qDebug() << cs[0]->property("book_id") << cs[0]->property("chapter") << cs[0]->property("media");
            });
        });
        if (!r)
            qDebug() << "Error:" << r.error();
    }
#endif

    Midoku::App app(&db);

    QQmlApplicationEngine engine;
    engine.addImageProvider("blob", new Midoku::QmlBlobImageProvider(&db));
    engine.setObjectOwnership(&app, QQmlEngine::CppOwnership);
    engine.rootContext()->setContextProperty("app", &app);
    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    fflush(stdout);
    fflush(stderr);

    return qapp.exec();
    //return 0;
}
