#pragma once

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

class QtThemeManager;

class QtSettingsUI : public QWidget {
    Q_OBJECT

  public:
    explicit QtSettingsUI(QWidget* parent = nullptr);
    ~QtSettingsUI() override = default;

    void applyTheme();
    void refresh2FAStatus();

  private slots:
    void onEnable2FAClicked();
    void onDisable2FAClicked();

  private:
    void setupUI();
    void update2FAStatus();
    void updateStyles();

    QtThemeManager* m_themeManager;
    QVBoxLayout* m_mainLayout;
    QScrollArea* m_scrollArea;
    QWidget* m_centerContainer;
    QLabel* m_titleLabel;
    QLabel* m_walletPlaceholder;
    QComboBox* m_themeSelector;

    QLabel* m_2FATitleLabel;
    QLabel* m_2FAStatusLabel;
    QPushButton* m_enable2FAButton;
    QPushButton* m_disable2FAButton;
    QLabel* m_2FADescriptionLabel;
};
