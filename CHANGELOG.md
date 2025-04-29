# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.2.0] - 2025-04-25
### Added
- Time-varying rainfall configuration
- Adaptive drainage factors for improved flow modeling
- Interactive pan/zoom controls for visualizations
- Runtime performance monitoring

### Changed
- Enhanced depression filling algorithm
- Optimized flow accumulation calculations
- Improved visualization rendering pipeline
- Migrated to Qt 6.10.0

### Fixed
- Memory leak in DEM loading
- Grid boundary condition handling
- NoData value processing in GeoTIFF files
- UI responsiveness during long simulations

## [0.1.0] - 2025-03-15
### Added
- Initial release with basic functionality
- DEM file loading support (GeoTIFF, CSV)
- Water flow simulation using Manning's equation
- Basic visualization system
- Manual and automatic outlet selection
- Real-time simulation controls
- Basic export capabilities

### Known Issues
- Performance degradation with large DEMs
- Limited error recovery options
- Basic visualization options only