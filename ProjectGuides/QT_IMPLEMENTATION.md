# Qt UI Framework Implementation

## Overview

This document describes the Qt UI framework implementation for CriptoGualet, providing a modern, themeable interface with enhanced user experience.

## Architecture

### Theme System
- **QtThemeManager**: Centralized theme management with 4 built-in themes
  - Crypto Dark (default)
  - Crypto Light
  - Standard Dark
  - Standard Light

### Components
- **CriptoGualetQt**: Main application window with navigation
- **QtLoginUI**: Modern login interface with theme selector
- **QtWalletUI**: Feature-rich wallet interface with card-based layout

## Features

### Theming
- Dynamic theme switching without restart
- Consistent color schemes across all components
- Typography management with proper font sizing
- CSS-like styling with Qt StyleSheets

### UI Components
- Modern card-based layouts
- Responsive design with scroll areas
- Professional button styling with hover effects
- Bitcoin address display with copy functionality
- Transaction history display area

### Security Integration
- Secure password handling with Qt's built-in password fields
- Integration with existing Auth system
- Proper memory management for sensitive data

## Building with Qt

### Prerequisites
1. Install Qt6 from https://qt.io
2. Ensure Qt6 is in your CMAKE_PREFIX_PATH

### Build Commands
```bash
cmake -S . -B build -DBUILD_GUI_QT=ON
cmake --build build --config Release
```

## Theme Customization

### Adding New Themes
1. Add theme enum to `ThemeType` in QtThemeManager.h
2. Implement setup function in QtThemeManager.cpp
3. Add theme option to UI selector

### Color Scheme Structure
- Primary/Secondary colors for surfaces
- Background colors for main areas
- Text colors with proper contrast
- Accent colors for interactive elements
- Status colors (error, success, warning)

## File Structure

```
include/
├── CriptoGualetQt.h      # Main application
├── QtLoginUI.h           # Login interface
├── QtWalletUI.h          # Wallet interface
└── QtThemeManager.h      # Theme system

src/
├── CriptoGualetQt.cpp    # Main implementation
├── QtLoginUI.cpp         # Login implementation
├── QtWalletUI.cpp        # Wallet implementation
└── QtThemeManager.cpp    # Theme implementation
```

## Usage

### Running the Qt Application
```bash
./build/src/Release/CriptoGualetQt.exe
```

### Theme Selection
- Use the dropdown in login screen
- Use Theme menu in main application
- Themes persist during session

### Features Demonstration
- Login/Registration with existing backend
- Bitcoin address generation and display
- Copy address to clipboard
- Demo balance and transaction display
- Secure logout functionality

## Integration Notes

- Seamlessly integrates with existing Auth system
- Uses same User structure and validation
- Compatible with existing Bitcoin address generation
- Maintains all security features from Win32 version

## Future Enhancements

- Custom theme creation interface
- Theme persistence across sessions
- Additional UI animations
- Enhanced transaction history
- Multi-language support
- Dark/light mode auto-detection