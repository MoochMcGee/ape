// Copyright 2018 Ape Emulator Project
// Licensed under GPLv3+
// Refer to the LICENSE file included.

#include <string>

#include <QMainWindow>
#include <QString>

class QMenuBar;

class MainWindow : public QMainWindow
{
public:
  explicit MainWindow(const std::string&& path = "");
  ~MainWindow();

private:
  void CreateWidgets();
  void ConnectWidgets();

  QString GetQuote() const;

  void OpenFile();
  void ShowAbout();

  void StartFile(const QString& path);

  QMenuBar* m_menu_bar;
};