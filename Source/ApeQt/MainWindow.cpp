// Copyright 2018 Ape Emulator Project
// Licensed under GPLv3+
// Refer to the LICENSE file included.

#include "ApeQt/MainWindow.h"

#include <memory>
#include <random>

#include <QFileDialog>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>

#include "ApeQt/Debugger/CodeWidget.h"
#include "ApeQt/Debugger/RegisterWidget.h"
#include "ApeQt/QueueOnObject.h"
#include "ApeQt/TTYWidget.h"

#include "Core/HW/FloppyDrive.h"
#include "Core/Memory.h"

#include "Version.h"

QString MainWindow::GetQuote() const
{
  const QStringList list{tr("Less FPS than DOSBox"),
                         tr("Garuantee void if opened"),
                         tr("May contain traces of nuts"),
                         tr("NOP? NOP!"),
                         tr("Realest Mode of them all"),
                         tr("640K ought to be enough for everyone"),
                         tr("Just works (sometimes)"),
                         tr("MOV UP, ME ; Scotty"),
                         tr("Big Blue is coming for you"),
                         tr("Crashes in your general direction")};

  std::random_device rd;
  std::default_random_engine engine(rd());
  std::uniform_int_distribution<int> rnd(0, list.size() - 1);

  return list[rnd(engine)];
}

MainWindow::MainWindow(const std::string&& path)
{
  Core::Machine::Init();

  setWindowTitle(tr("Ape %1 - %2!").arg(VERSION_STRING).arg(GetQuote()));

  CreateWidgets();
  ConnectWidgets();

  resize(800, 600);

  StartFile(QString::fromStdString(path));

  Core::CPU::RegisterStateChangedCallback(
      [this](Core::CPU::State s) { OnMachineStateChanged(s); });
}

MainWindow::~MainWindow() { StopMachine(); }

void MainWindow::CreateWidgets()
{
  m_menu_bar = new QMenuBar;

  auto* file_menu = m_menu_bar->addMenu(tr("File"));

  file_menu->addAction(tr("Open..."), this, &MainWindow::OpenFile,
                       QKeySequence("Ctrl+O"));
  file_menu->addAction(tr("Exit"), this, &MainWindow::close,
                       QKeySequence("Alt+F4"));

  auto* machine_menu = m_menu_bar->addMenu(tr("Machine"));

  m_machine_stop = machine_menu->addAction(
      tr("Stop"), this, &MainWindow::StopMachine, QKeySequence("Ctrl+H"));

  m_machine_pause = machine_menu->addAction(
      tr("Pause"), this, &MainWindow::PauseMachine, QKeySequence("Ctrl+P"));

  m_machine_stop->setEnabled(false);
  m_machine_pause->setEnabled(false);

  auto* help_menu = m_menu_bar->addMenu(tr("Help"));

  help_menu->addAction(tr("About..."), this, &MainWindow::ShowAbout);

  setCentralWidget(new TTYWidget);

  setMenuBar(m_menu_bar);

  m_status_bar = new QStatusBar;
  m_status_label = new QLabel(tr("Ready"));

  m_status_bar->addPermanentWidget(m_status_label);

  ShowStatus(tr("Welcome to Ape!"), 5000);

  setStatusBar(m_status_bar);

  auto* code_widget = new CodeWidget;
  auto* register_widget = new RegisterWidget;

  addDockWidget(Qt::LeftDockWidgetArea, code_widget);
  addDockWidget(Qt::LeftDockWidgetArea, register_widget);

  tabifyDockWidget(code_widget, register_widget);
}

void MainWindow::ConnectWidgets() {}

void MainWindow::OpenFile()
{
  const QString& path =
      QFileDialog::getOpenFileName(this, tr("Open File"), QString(),
                                   tr("Floppy Image(*.img);; COM File(*.com)"));

  if (path.isEmpty())
    return;

  Core::Machine::Stop();

  if (m_thread.joinable())
    m_thread.join();

  StartFile(path);
}

void MainWindow::StartFile(const QString& path)
{
  if (path.isEmpty())
    return;

  if (path.endsWith(".com", Qt::CaseInsensitive)) {
    m_thread = std::thread([this, path] {
      try {
        Core::Machine::BootCOM(path.toStdString());
      } catch (Core::CPU::CPUException& e) {
        HandleException(e);
      }
    });
    return;
  }

  if (!Core::HW::FloppyDrive::Insert(path.toStdString())) {
    QMessageBox::critical(this, tr("Error"), tr("Failed to mount floppy!"));
    return;
  }

  if (!Core::HW::FloppyDrive::IsBootable()) {
    QMessageBox::critical(this, tr("Error"),
                          tr("The provided disk is not bootable!"));
    return;
  }

  m_thread = std::thread([this, path] {
    try {
      Core::Machine::BootFloppy();
    } catch (Core::CPU::CPUException& e) {
      HandleException(e);
    }
  });
}

void MainWindow::StopMachine()
{
  Core::Machine::Stop();

  if (m_thread.joinable())
    m_thread.join();
}

void MainWindow::PauseMachine()
{
  const bool paused = Core::CPU::IsPaused();
  ShowStatus(paused ? tr("Resumed") : tr("Paused"));
  m_machine_pause->setText(paused ? tr("Resume") : tr("Pause"));

  Core::Machine::Pause();
}

void MainWindow::OnMachineStateChanged(Core::CPU::State state)
{
  QString msg;
  switch (state) {
  case Core::CPU::State::Stopped:
    msg = tr("Stopped");
    break;
  case Core::CPU::State::Running:
    msg = tr("Running");
    break;
  case Core::CPU::State::Paused:
    msg = tr("Paused");
    break;
  }

  ShowStatus(msg);

  QueueOnObject(this, [this, state, msg] {
    m_machine_stop->setEnabled(state != Core::CPU::State::Stopped);
    m_machine_pause->setEnabled(state != Core::CPU::State::Stopped);
    m_status_label->setText(msg);
  });
}

void MainWindow::HandleException(Core::CPU::CPUException e)
{
  QueueOnObject(this, [this, e] {
    QMessageBox::critical(
        this, tr("Error"),
        tr("A fatal error exception and emulation cannot continue:\n\n%1")
            .arg(QString::fromStdString(e.what())));
  });

  ShowStatus("Crashed :(");
}

void MainWindow::ShowStatus(const QString& message, int timeout)
{
  QueueOnObject(this, [this, message, timeout] {
    m_status_bar->showMessage(message, timeout);
  });
}

void MainWindow::ShowAbout()
{
  QMessageBox::information(
      this, tr("About Ape"),
      tr("Version: %1\n\nApe is an experimental IBM PC emulator written in "
         "C++17.\n\nApe is "
         "licensed under the GNU GPL v3 or any later version at your "
         "option. See LICENSE.\n\n(c) Ape Emulator Project, 2018")
          .arg(VERSION_STRING));
}
