# Doxygen Extensions

This section contains advanced Doxygen documentation features and extensions for the 
HardFOC Interface Wrapper project.

## Features

Our Doxygen documentation includes several modern enhancements:

- **🌓 Automatic Dark Mode** - Detects system preference and switches automatically
- **📋 Copy Buttons** - Hover over code blocks to copy them to clipboard
- **🔗 Paragraph Links** - Click the ¶ symbol to get direct links to sections
- **📑 Interactive TOC** - Dynamic table of contents with progress highlighting
- **📑 Tabs** - Organize content in tabbed interfaces
- **📱 Responsive Design** - Works perfectly on desktop and mobile devices

## Quick Start

The Doxygen documentation is automatically generated from the source code and includes:

1. **API Reference** - Complete documentation for all base classes and ESP32 implementations
2. **Code Examples** - Comprehensive test examples showing proper usage
3. **Interactive Features** - Modern UI with dark mode and copy functionality

## Structure

- **[Configuration](../_config/doxygen-extensions/)** - Doxygen configuration and setup files
- **[Submodule](../_config/doxygen-extensions/doxygen-awesome-css/)** - Doxygen-awesome-css theme files

## Technical Details

The documentation system uses:

- **Doxygen** - For generating API documentation from source code
- **doxygen-awesome-css** - Modern CSS theme with dark mode support
- **Custom Extensions** - JavaScript enhancements for better user experience
- **Automatic Detection** - System preference detection for theme switching

## Usage

To generate the documentation locally:

```bash
doxygen Doxyfile
```

The generated documentation will be available in the `docs/doxygen/` directory.

## Customization

The documentation theme and features can be customized by modifying:

- `Doxyfile` - Main Doxygen configuration
- `_config/doxygen-extensions/doxygen-awesome-css/` - Theme CSS and JavaScript files

## Related Documentation

- [API Reference](api/) - Base class documentation
- [ESP32 Implementations](esp_api/) - ESP32-specific implementations
- [Utilities](utils/) - Utility classes and helpers
