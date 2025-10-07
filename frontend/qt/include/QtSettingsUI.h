#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>

class QtThemeManager;

class QtSettingsUI : public QWidget {
    Q_OBJECT

public:
    explicit QtSettingsUI(QWidget *parent = nullptr);
    ~QtSettingsUI() = default;

    void applyTheme();

private:
    void setupUI();

    QtThemeManager *m_themeManager;
    QVBoxLayout *m_mainLayout;
    QLabel *m_titleLabel;
    QComboBox *m_themeSelector;
};
