#include <QApplication>
#include <QPushButton>
#include <QVBoxLayout>

int main(int argc, char* argv[])
{
    // Initialize Qt application and parse command-line arguments
    const QApplication app(argc, argv);

    // Create main window widget
    QWidget window;
    window.setWindowTitle("Qt vcpkg CMake Demo");
    window.resize(400, 300);

    // Create vertical layout and a push button
    QVBoxLayout* layout = new QVBoxLayout(&window);
    QPushButton* button = new QPushButton("Hello from Qt!", &window);
    layout->addWidget(button);

    // Connect button click signal to application quit slot
    QObject::connect(button, &QPushButton::clicked, &app, &QApplication::quit);

    // Show the window and start the Qt event loop
    window.show();
    return QApplication::exec();
}
