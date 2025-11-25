#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>

class QtThemeManager;

class QtSettingsUI : public QWidget {
    Q_OBJECT

public:
    explicit QtSettingsUI(QWidget *parent = nullptr);
    ~QtSettingsUI() override = default;

    void applyTheme();
    void refresh2FAStatus(); // Refresh 2FA status when settings page is shown

private slots:
    void onEnable2FAClicked();
    void onDisable2FAClicked();

private:
    void setupUI();
    void update2FAStatus();

    QtThemeManager *m_themeManager;
    QVBoxLayout *m_mainLayout;
    QLabel *m_titleLabel;
    QLabel *m_walletPlaceholder;
    QComboBox *m_themeSelector;
    
    // 2FA controls
    QLabel *m_2FAStatusLabel;
    QPushButton *m_enable2FAButton;
    QPushButton *m_disable2FAButton;
    QLabel *m_2FADescriptionLabel;
};
