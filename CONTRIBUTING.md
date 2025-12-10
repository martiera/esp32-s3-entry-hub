# Contributing to ESP32-S3 Entry Hub

Thank you for your interest in contributing! This document provides guidelines for contributing to the project.

## Code of Conduct

- Be respectful and inclusive
- Provide constructive feedback
- Focus on what is best for the community

## How to Contribute

### Reporting Bugs

1. Check if the bug has already been reported in Issues
2. If not, create a new issue with:
   - Clear title and description
   - Steps to reproduce
   - Expected vs actual behavior
   - ESP32-S3 board version
   - PlatformIO/Arduino version
   - Serial output logs

### Suggesting Enhancements

1. Check if the enhancement has been suggested
2. Create an issue describing:
   - Use case and motivation
   - Proposed solution
   - Alternative solutions considered

### Pull Requests

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Make your changes
4. Test thoroughly
5. Commit with clear messages (`git commit -m 'Add amazing feature'`)
6. Push to your fork (`git push origin feature/amazing-feature`)
7. Open a Pull Request

#### PR Guidelines

- Follow existing code style
- Add comments for complex logic
- Update documentation if needed
- Test on actual hardware when possible
- Keep PRs focused on a single feature/fix

### Code Style

- Use consistent indentation (4 spaces)
- Descriptive variable and function names
- Add comments for complex sections
- Follow Arduino/C++ best practices

### Testing

- Test on ESP32-S3 hardware
- Verify web interface in multiple browsers
- Test MQTT connectivity
- Check Home Assistant integration
- Monitor memory usage

## Development Setup

See SETUP.md for detailed setup instructions.

## Project Structure

```
esp32-s3-entry-hub/
├── include/           # Header files
├── src/              # Source code
├── data/             # Web files and config
├── lib/              # Custom libraries
├── scripts/          # Build scripts
└── test/             # Tests
```

## Areas for Contribution

### High Priority
- TensorFlow Lite Micro integration
- Actual Porcupine library integration
- LVGL display implementation
- More voice command templates
- Weather API integration
- Calendar sync implementation

### Medium Priority
- Unit tests
- More Home Assistant entities
- Additional scene templates
- Mobile app
- Backup/restore functionality

### Documentation
- More example configurations
- Video tutorials
- Wiring diagrams
- Troubleshooting guides

## Questions?

Feel free to open an issue for questions or discussions.

Thank you for contributing! 🎉
