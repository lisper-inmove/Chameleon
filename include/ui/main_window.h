#pragma once

#include <QMainWindow>

namespace chameleon {

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
};

} // namespace chameleon
