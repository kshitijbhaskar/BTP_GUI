# Sample DEM Files

This directory contains sample Digital Elevation Model (DEM) files for testing and demonstration.

## Prerequisites

1. **Python Dependencies**
   ```bash
   pip install rasterio numpy
   ```

2. **Data Format Requirements**
   - GeoTIFF files must have square pixels
   - CSV files should be comma-separated with NO_DATA as -999999.0
   - Check coordinate system for GeoTIFF files

## File Formats

### CSV Format
- `sample_dem.csv`: 5x5 grid with 1m resolution
  - Simple example for testing basic functionality
  - Contains NO_DATA values (-999999.0)
  - Gentle slope from north to south
  - Perfect for initial algorithm validation

### GeoTIFF Format
- `DEM_Central_Park.tif`: Central Park, New York (1m resolution)
  - High resolution: 843x742 cells
  - Complex urban drainage patterns
  - Good for testing automatic outlet detection
  - Actual elevation range: 10m to 45m

- `DEM_Central_Park(10m).tif`: Same area at 10m resolution
  - Coarse resolution: 85x75 cells
  - Performance testing reference
  - Useful for resolution comparison studies
  - Quick testing and development

- `DEM_IITKGP.tif`: IIT Kharagpur campus
  - Resolution: 2m
  - Size: 1024x1024 cells
  - Mixed built/natural environment
  - Ideal for drainage routing tests
  - Contains building footprints

- `DEM_NTPC_Vindhayanagar.tif`: Power plant site
  - Resolution: 5m
  - Size: 512x512 cells
  - Industrial site layout
  - Multiple artificial depressions
  - Tests depression filling algorithms
  - Engineered drainage systems

- `DEM_Gray_Haven.tif`: Residential area
  - Resolution: 2m
  - Size: 756x654 cells
  - Natural channel networks
  - Suburban development impacts
  - Mixed slope conditions
  - Good for flow accumulation tests

- `DEM_Amba.tif`: Natural watershed
  - Resolution: 10m
  - Size: 2048x2048 cells
  - Large natural catchment
  - Complex stream networks
  - Memory handling test case
  - Full watershed analysis

## Format Conversion

The `dem_to_csv.py` script helps convert GeoTIFF DEMs to CSV format:

```python
# Usage:
# 1. Edit dem_path variable in script
# 2. Run: python dem_to_csv.py
# 3. Output will be saved as converted_dem.csv

import rasterio
import numpy as np

# Path to your DEM file (GeoTIFF or similar)
dem_path = 'DEM_NTPC_Vindhayanagar.tif'

with rasterio.open(dem_path) as src:
    dem = src.read(1)
    transform = src.transform
    
    # Extract resolution from the affine transform
    res_x = transform.a  # pixel width (meters)
    res_y = -transform.e  # pixel height (meters)
    
    print(f"DEM resolution: {res_x:.2f} m (X) x {res_y:.2f} m (Y)")
    
    # Export only the elevation (z) values to CSV
    np.savetxt('converted_dem.csv', dem, delimiter=',', fmt='%.6f')
    print("Saved elevation grid to 'converted_dem.csv'")
```

## Usage Tips

1. Start with `sample_dem.csv` for basic testing
   - Simple verification of algorithms 
   - Quick iteration on changes
   - Easy to inspect results

2. Use `DEM_Central_Park(10m).tif` for development
   - Reasonable size for quick tests
   - Real-world terrain features
   - Fast processing time

3. Progress to higher resolution files
   - `DEM_IITKGP.tif` for mixed environments
   - `DEM_Gray_Haven.tif` for natural channels
   - Full resolution Central Park for detail

4. Use `DEM_NTPC_Vindhayanagar.tif` for special cases
   - Depression filling validation
   - Artificial drainage systems
   - Complex flow routing

5. Test large-scale performance with `DEM_Amba.tif`
   - Memory management
   - Processing optimization
   - UI responsiveness

## Data Sources

- USGS Earth Explorer for Central Park DEMs
- RTK GPS surveys for IITKGP campus
- LiDAR data for NTPC industrial site
- Public domain SRTM data for other samples

## File Details

| File | Resolution | Size | Memory* | Features |
|------|------------|------|---------|-----------|
| sample_dem.csv | 1m | 5x5 | <1MB | Basic test case |
| DEM_Central_Park.tif | 1m | 843x742 | ~5MB | Urban terrain |
| DEM_Central_Park(10m).tif | 10m | 85x75 | <1MB | Quick testing |
| DEM_IITKGP.tif | 2m | 1024x1024 | ~8MB | Campus terrain |
| DEM_NTPC_Vindhayanagar.tif | 5m | 512x512 | ~2MB | Industrial site |
| DEM_Gray_Haven.tif | 2m | 756x654 | ~4MB | Residential area |
| DEM_Amba.tif | 10m | 2048x2048 | ~32MB | Large watershed |

*Memory estimates are for raw elevation data, actual usage will be higher due to processing buffers