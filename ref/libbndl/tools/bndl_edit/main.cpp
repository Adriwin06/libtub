#include "editor.hpp"
#include <QApplication>
#include <QCommandLineParser>

int main(int argc,char** argv)
{
	QApplication app(argc, argv);
	QCoreApplication::setOrganizationName("Stephan Vedder");
	QCoreApplication::setApplicationName("Bundle Editor");
	QCoreApplication::setApplicationVersion(QT_VERSION_STR);
	QCommandLineParser parser;
	parser.setApplicationDescription(QCoreApplication::applicationName());
	parser.addHelpOption();
	parser.addVersionOption();
	parser.addPositionalArgument("file", "The file to open.");
	parser.process(app);

	Editor window;
	window.show();

	return app.exec();
}