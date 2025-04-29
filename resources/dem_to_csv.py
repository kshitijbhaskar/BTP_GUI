#!/usr/bin/env python3
"""
DEM to CSV Converter
Converts GeoTIFF Digital Elevation Models to CSV format for BTP GUI.

Usage:
    python dem_to_csv.py input.tif [output.csv]
    python dem_to_csv.py --help

Example:
    python dem_to_csv.py DEM_Gray_Haven.tif
"""

import sys
import os
import argparse
import rasterio
import numpy as np
from rasterio.errors import RasterioError

def get_resolution(transform):
    """Extract pixel resolution from the transform matrix."""
    # For most GeoTIFFs, resolution is stored in transform[0] and transform[4]
    res_x = abs(transform[0])  # Element (0,0) for X scaling
    res_y = abs(transform[4])  # Element (1,1) for Y scaling
    return res_x, res_y

def convert_dem(input_path, output_path=None):
    """Convert GeoTIFF DEM to CSV format.
    
    Args:
        input_path: Path to input GeoTIFF file
        output_path: Optional path for output CSV file
    """
    try:
        # Default output name if not specified
        if not output_path:
            base = os.path.splitext(input_path)[0]
            output_path = f"{base}.csv"
            
        print(f"Reading DEM: {input_path}")
        with rasterio.open(input_path) as src:
            # Read the entire first band
            dem = src.read(1)
            
            # Get resolution from transform matrix
            res_x, res_y = get_resolution(src.transform)
            
            # Validate resolution
            if abs(res_x - res_y) > 1e-6:
                print(f"Warning: Non-square pixels detected!")
                print(f"X resolution: {res_x:.3f}m")
                print(f"Y resolution: {res_y:.3f}m")
                print("Using X resolution for output")
            else:
                print(f"DEM resolution: {res_x:.2f}m")
                
            # Print grid dimensions
            rows, cols = dem.shape
            print(f"Grid size: {rows}x{cols} cells")
            
            # Handle nodata values
            nodata_val = src.nodata
            if nodata_val is not None:
                print(f"Converting NoData value {nodata_val} to -999999.0")
                dem[dem == nodata_val] = -999999.0
            else:
                print("No NoData value specified in input file")
                
            # Check for invalid values
            invalid_mask = ~np.isfinite(dem)
            if np.any(invalid_mask):
                print("Warning: Found invalid values (NaN/inf), replacing with -999999.0")
                dem[invalid_mask] = -999999.0
            
            # Save to CSV with high precision
            print(f"Writing CSV: {output_path}")
            np.savetxt(output_path, dem, delimiter=',', fmt='%.6f')
            
            # Verify the output
            try:
                test_read = np.loadtxt(output_path, delimiter=',')
                if test_read.shape == dem.shape:
                    print("Conversion complete - output verified!")
                    
                    # Print memory estimate
                    mem_mb = (dem.size * dem.itemsize) / (1024*1024)
                    print(f"Estimated memory usage: {mem_mb:.1f}MB")
                else:
                    print("Error: Output dimensions don't match input!")
            except Exception as e:
                print(f"Error verifying output file: {e}")
            
    except RasterioError as e:
        print(f"Error reading DEM file: {e}")
        print("Please check if GDAL is properly installed")
        sys.exit(1)
    except Exception as e:
        print(f"Unexpected error: {e}")
        print("Make sure you have required dependencies:")
        print("pip install rasterio numpy")
        sys.exit(1)

def main():
    parser = argparse.ArgumentParser(
        description="Convert GeoTIFF DEM to CSV format",
        formatter_class=argparse.RawDescriptionHelpFormatter)
    
    parser.add_argument("input", 
                       help="Input GeoTIFF file (e.g., DEM_Gray_Haven.tif)")
    parser.add_argument("output", 
                       nargs='?', 
                       help="Output CSV file (optional)")
    
    if len(sys.argv) == 1:
        parser.print_help()
        sys.exit(1)
    
    args = parser.parse_args()
    
    if not os.path.exists(args.input):
        print(f"Error: Input file not found: {args.input}")
        sys.exit(1)
        
    convert_dem(args.input, args.output)

if __name__ == "__main__":
    main()
