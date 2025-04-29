# Contributing to Advanced Hydrological Simulation Application

First off, thank you for considering contributing to this project! 

## Development Process

1. **Setting Up Development Environment**
   - Install required dependencies (Qt 6.10.0, GDAL, VS2022)
   - Configure environment variables as specified in README.md
   - Build project in Debug mode for development

2. **Code Style**
   - Follow C++17 standards
   - Use Qt best practices for UI components
   - Keep functions focused and well-documented
   - Maintain consistent naming conventions:
     - Classes: PascalCase
     - Methods: camelCase
     - Variables: camelCase
     - Constants: UPPER_SNAKE_CASE

3. **Documentation Requirements**
   - Document all public APIs using Doxygen format
   - Include parameter descriptions and units
   - Add examples for complex functionality
   - Update README.md for major changes

4. **Testing Guidelines**
   - Write unit tests for new functionality
   - Verify mass conservation in flow calculations
   - Test with various DEM resolutions
   - Validate boundary conditions
   - Check memory usage with large datasets

5. **Pull Request Process**
   - Create feature branch from main
   - Keep changes focused and atomic
   - Include test cases
   - Update documentation
   - Ensure CI passes

## Common Tasks

### Adding New Features
1. Discuss in Issues first
2. Create feature branch
3. Implement with tests
4. Update documentation
5. Submit PR

### Bug Fixes
1. Create reproduction case
2. Write failing test
3. Fix bug
4. Verify test passes
5. Submit PR

### Performance Improvements
1. Profile current behavior
2. Document baseline metrics
3. Implement optimization
4. Verify improvements
5. Include benchmarks

## Version Control

### Branch Naming
- feature/description
- bugfix/issue-number
- perf/component-name
- doc/topic

### Commit Messages
- Use present tense
- Be descriptive
- Reference issues

Example:
```
feat: Add time-varying rainfall support

- Implement rainfall schedule UI
- Add interpolation logic
- Update documentation
- Add unit tests

Fixes #123
```

## Code Review Process

### Reviewer Guidelines
- Check code style compliance
- Verify documentation completeness
- Validate test coverage
- Review performance implications
- Ensure backward compatibility

### Author Responsibilities
- Respond to feedback promptly
- Keep PR scope focused
- Update based on reviews
- Maintain PR description

## Release Process

1. Version Bumping
   - Update version numbers
   - Update changelog
   - Review deprecations

2. Testing Requirements
   - All tests passing
   - Performance benchmarks
   - Memory leak checks
   - Large DEM testing

3. Documentation Updates
   - Feature documentation
   - API changes
   - Migration guides
   - Example updates

## Contact

For questions or clarifications:
- Create an issue
- Email: kshitijbhaskar22@gmail.com