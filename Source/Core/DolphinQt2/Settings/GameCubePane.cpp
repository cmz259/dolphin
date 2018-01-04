// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Settings/GameCubePane.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/EXI/EXI.h"
#include "Core/HW/GCMemcard/GCMemcard.h"
#include "DolphinQt2/Config/Mapping/MappingWindow.h"

constexpr int SLOT_A_INDEX = 0;
constexpr int SLOT_B_INDEX = 1;
constexpr int SLOT_COUNT = 3;

constexpr int EXP_MEMORYCARD_INDEX = 2;
constexpr int EXP_GECKO_INDEX = 4;

GameCubePane::GameCubePane()
{
  CreateWidgets();
  ConnectWidgets();

  LoadSettings();
}

void GameCubePane::CreateWidgets()
{
  QVBoxLayout* layout = new QVBoxLayout;

  // IPL Settings
  QGroupBox* ipl_box = new QGroupBox(tr("IPL Settings"));
  QGridLayout* ipl_layout = new QGridLayout;
  ipl_box->setLayout(ipl_layout);

  m_skip_main_menu = new QCheckBox(tr("Skip Main Menu"));
  m_override_language_ntsc = new QCheckBox(tr("Override Language on NTSC Games"));
  m_language_combo = new QComboBox;

  // Add languages
  for (const auto& language :
       {tr("English"), tr("German"), tr("French"), tr("Spanish"), tr("Italian"), tr("Dutch")})
    m_language_combo->addItem(language);

  ipl_layout->addWidget(m_skip_main_menu, 0, 0);
  ipl_layout->addWidget(new QLabel(tr("System Language:")), 1, 0);
  ipl_layout->addWidget(m_language_combo, 1, 1);
  ipl_layout->addWidget(m_override_language_ntsc, 2, 0);

  // Device Settings
  QGroupBox* device_box = new QGroupBox(tr("Device Settings"));
  QGridLayout* device_layout = new QGridLayout;
  device_box->setLayout(device_layout);

  m_slot_combos[0] = new QComboBox;
  m_slot_combos[1] = new QComboBox;
  m_slot_combos[2] = new QComboBox;

  m_slot_buttons[0] = new QPushButton(tr("..."));
  m_slot_buttons[1] = new QPushButton(tr("..."));

  m_slot_buttons[0]->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  m_slot_buttons[1]->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  const QString i10n_nothing = tr("<Nothing>");
  const QString i10n_dummy = tr("Dummy");

  // Add slot devices

  for (const auto& device : {i10n_nothing, i10n_dummy, tr("Memory Card"), tr("GCI Folder"),
                             tr("USB Gecko"), tr("Advance Game Port"), tr("Microphone")})
  {
    m_slot_combos[0]->addItem(device);
    m_slot_combos[1]->addItem(device);
  }

  // Add SP1 devices

  for (const auto& device : {i10n_nothing, i10n_dummy, tr("Broadband Adapter")})
  {
    m_slot_combos[2]->addItem(device);
  }

  device_layout->addWidget(new QLabel(tr("Slot A:")), 0, 0);
  device_layout->addWidget(m_slot_combos[0], 0, 1);
  device_layout->addWidget(m_slot_buttons[0], 0, 2);
  device_layout->addWidget(new QLabel(tr("Slot B:")), 1, 0);
  device_layout->addWidget(m_slot_combos[1], 1, 1);
  device_layout->addWidget(m_slot_buttons[1], 1, 2);
  device_layout->addWidget(new QLabel(tr("SP1:")), 2, 0);
  device_layout->addWidget(m_slot_combos[2], 2, 1);

  layout->addWidget(ipl_box);
  layout->addWidget(device_box);

  setLayout(layout);
}

void GameCubePane::ConnectWidgets()
{
  // IPL Settings
  connect(m_skip_main_menu, &QCheckBox::stateChanged, this, &GameCubePane::SaveSettings);
  connect(m_language_combo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          this, &GameCubePane::SaveSettings);
  connect(m_override_language_ntsc, &QCheckBox::stateChanged, this, &GameCubePane::SaveSettings);

  // Device Settings
  for (int i = 0; i < SLOT_COUNT; i++)
  {
    connect(m_slot_combos[i],
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &GameCubePane::SaveSettings);
    if (i <= SLOT_B_INDEX)
    {
      connect(m_slot_buttons[i], &QPushButton::pressed, this, [this, i] { OnConfigPressed(i); });
    }
  }
}

void GameCubePane::OnConfigPressed(int slot)
{
  QString filter;
  bool memcard = false;

  switch (m_slot_combos[slot]->currentIndex())
  {
  // Memory card
  case 2:
    filter = tr("GameCube Memory Cards (*.raw,*.gcp)");
    memcard = true;
    break;
  // Advance Game Port
  case 5:
    filter = tr("Game Boy Advance Carts (*.gba)");
    memcard = false;
    break;
  // Microphone
  case 6:
    MappingWindow(this, MappingWindow::Type::MAPPING_GC_MICROPHONE, slot).exec();
    return;
  }

  QString filename =
      QFileDialog::getOpenFileName(this, tr("Choose a file to open"), QStringLiteral(""), filter);

  if (filename.isEmpty() || !File::Exists(filename.toStdString()))
    return;

  QString path_abs = QFileInfo(filename).absolutePath();

  // Memcard validity checks
  if (memcard)
  {
    GCMemcard mc(filename.toStdString());

    if (!mc.IsValid())
    {
      QMessageBox::critical(this, tr("Error"), tr("Cannot use that file as a memory card.\n%s\n"
                                                  "is not a valid GameCube memory card file")
                                                   .arg(filename));
      return;
    }

    QString path_b =
        QFileInfo(QString::fromStdString(slot == 0 ? SConfig::GetInstance().m_strMemoryCardB :
                                                     SConfig::GetInstance().m_strMemoryCardA))
            .absolutePath();

    if (path_abs == path_b)
    {
      QMessageBox::critical(this, tr("Error"), tr("The same file can't be used in both slots."));
      return;
    }
  }

  if (memcard)
  {
    if (slot == SLOT_A_INDEX)
    {
      SConfig::GetInstance().m_strMemoryCardA = path_abs.toStdString();
    }
    else
    {
      SConfig::GetInstance().m_strMemoryCardB = path_abs.toStdString();
    }
  }
  else
  {
    if (slot == SLOT_A_INDEX)
    {
      SConfig::GetInstance().m_strGbaCartA = path_abs.toStdString();
    }
    else
    {
      SConfig::GetInstance().m_strGbaCartB = path_abs.toStdString();
    }
  }
}

void GameCubePane::LoadSettings()
{
  const SConfig& params = SConfig::GetInstance();

  // IPL Settings
  m_skip_main_menu->setChecked(params.bHLE_BS2);
  m_language_combo->setCurrentIndex(params.SelectedLanguage);
  m_override_language_ntsc->setChecked(params.bOverrideGCLanguage);

  bool have_menu = false;

  for (const std::string& dir : {USA_DIR, JAP_DIR, EUR_DIR})
  {
    const auto& path = DIR_SEP + dir + DIR_SEP GC_IPL;
    if (File::Exists(File::GetUserPath(D_GCUSER_IDX) + path) ||
        File::Exists(File::GetUserPath(D_GCUSER_IDX) + path))
    {
      have_menu = true;
      break;
    }
  }

  m_skip_main_menu->setEnabled(have_menu);
  m_skip_main_menu->setToolTip(have_menu ? QStringLiteral("") :
                                           tr("Put Main Menu roms in User/GC/{region}."));

  // Device Settings

  for (int i = 0; i < SLOT_COUNT; i++)
  {
    int index = 0;
    switch (SConfig::GetInstance().m_EXIDevice[i])
    {
    case ExpansionInterface::EXIDEVICE_NONE:
      index = 0;
      break;
    case ExpansionInterface::EXIDEVICE_DUMMY:
      index = 1;
      break;
    case ExpansionInterface::EXIDEVICE_MEMORYCARD:
      index = 2;
      break;
    case ExpansionInterface::EXIDEVICE_MEMORYCARDFOLDER:
      index = 3;
      break;
    case ExpansionInterface::EXIDEVICE_GECKO:
      index = 4;
      break;
    case ExpansionInterface::EXIDEVICE_AGP:
      index = 5;
      break;
    case ExpansionInterface::EXIDEVICE_MIC:
      index = 6;
      break;
    default:
      break;
    }

    if (i <= SLOT_B_INDEX)
    {
      bool has_config = (index == EXP_MEMORYCARD_INDEX || index > EXP_GECKO_INDEX);
      m_slot_buttons[i]->setEnabled(has_config);
    }

    m_slot_combos[i]->setCurrentIndex(index);
  }
}

void GameCubePane::SaveSettings()
{
  SConfig& params = SConfig::GetInstance();

  // IPL Settings
  params.bHLE_BS2 = m_skip_main_menu->isChecked();
  params.SelectedLanguage = m_language_combo->currentIndex();
  params.bOverrideGCLanguage = m_override_language_ntsc->isChecked();

  for (int i = 0; i < SLOT_COUNT; i++)
  {
    auto& dev = SConfig::GetInstance().m_EXIDevice[i];

    int index = m_slot_combos[i]->currentIndex();

    if (i <= SLOT_B_INDEX)
    {
      bool has_config = (index == 3 || index > 4);
      m_slot_buttons[i]->setEnabled(has_config);
    }

    switch (index)
    {
    case 0:
      dev = ExpansionInterface::EXIDEVICE_NONE;
      break;
    case 1:
      dev = ExpansionInterface::EXIDEVICE_DUMMY;
      break;
    case 2:
      dev = ExpansionInterface::EXIDEVICE_MEMORYCARD;
      break;
    case 3:
      dev = ExpansionInterface::EXIDEVICE_MEMORYCARDFOLDER;
      break;
    case 4:
      dev = ExpansionInterface::EXIDEVICE_GECKO;
      break;
    case 5:
      dev = ExpansionInterface::EXIDEVICE_AGP;
      break;
    case 6:
      dev = ExpansionInterface::EXIDEVICE_MIC;
      break;
    }

    if (Core::IsRunning() && SConfig::GetInstance().m_EXIDevice[i] != dev)
    {
      ExpansionInterface::ChangeDevice(
          (index == 1) ? 1 : 0,   // SlotB is on channel 1, slotA and SP1 are on 0
          dev,                    // The device enum to change to
          (index == 2) ? 2 : 0);  // SP1 is device 2, slots are device 0
    }

    SConfig::GetInstance().m_EXIDevice[i] = dev;
  }
  LoadSettings();
}
