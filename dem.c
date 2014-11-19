//
//  DEM.c
//  Created by Robby on 5/10/14.
//

#include <stdio.h>
#include <string>
#include "dem.h"

// DEM standard for all plates except Antartica
//#define NROWS 6000
//#define NCOLS 4800
//#define XDIM 0.00833333333333
//#define YDIM 0.00833333333333
//#define ULXMAP -89.3333333
//#define ULYMAP 90.0

struct demMeta {
    unsigned int nrows;
    unsigned int ncols;
    double ulxmap;
    double ulymap;
    double xdim;
    double ydim;
};
typedef struct demMeta demMeta;

demMeta loadHeader(string directory, string filename){
// coodinated to .HDR file standard (accompanies .DEM files)
    demMeta meta;
    string path = directory+filename+".HDR";
    FILE *file = fopen(path.c_str(), "r");
    char s1[20], s2[20];
    double d1;
    int i = 0;
    int cmp;
    printf("\nLoading %s\n(%s)\n╔════════════════════════════════\n", filename.c_str(), directory.c_str());
    do {
        cmp = fscanf(file,"%s %lf", s1, &d1);
        if(cmp == 1){
            fscanf(file,"%s", s2);
            printf("║ %s: %s\n", s1, s2);
        }
        else if(cmp > 1){
            printf("║ %s: %f\n", s1, d1);
        }
        if(i == 2) meta.nrows = d1;
        else if(i == 3) meta.ncols = d1;
        else if(i == 10) meta.ulxmap = d1;
        else if(i == 11) meta.ulymap = d1;
        else if(i == 12) meta.xdim = d1;
        else if(i == 13) meta.ydim = d1;
        i++;
    } while (cmp > 0);
    printf("╚════════════════════════════════\n");
    fclose(file);
    return meta;
}

unsigned long getByteOffset(float latitude, float longitude, demMeta meta){
    
    float plateWidth = meta.xdim * meta.ncols;  // in degrees, Longitude
    float plateHeight = meta.ydim * meta.nrows; // in degrees, Latitude
    
    if(longitude < meta.ulxmap || longitude > meta.ulxmap+plateWidth || latitude > meta.ulymap || latitude < meta.ulymap-plateHeight){
        printf("\nEXCEPTION: lat long exceeds plate boundary\n");
        return NULL;
    }
    double xOffset = (longitude-meta.ulxmap)/plateWidth;  // 0.0 - 1.0
    double yOffset = (meta.ulymap-latitude)/plateHeight;  // 0.0 - 1.0
    
    unsigned int byteX = xOffset*meta.ncols;
    unsigned int byteY = yOffset*meta.nrows;
    
    return (byteX + byteY*meta.ncols) * 2;  // * 2, each index is 2 bytes wide
}

void convertLatLonToXY(demMeta meta, float latitude, float longitude, unsigned int *x, unsigned int *y){
    
    double plateWidth = meta.xdim * meta.ncols;  // in degrees, Longitude
    double plateHeight = meta.ydim * meta.nrows; // in degrees, Latitude
    
    if(longitude < meta.ulxmap || longitude > meta.ulxmap+plateWidth || latitude > meta.ulymap || latitude < meta.ulymap-plateHeight){
        printf("\nEXCEPTION: lat long exceeds plate boundary\n");
        return;
    }
    double xOffset = (longitude-meta.ulxmap)/plateWidth;  // 0.0 - 1.0
    double yOffset = (meta.ulymap-latitude)/plateHeight;  // 0.0 - 1.0
    
    *x = xOffset*meta.ncols;
    *y = yOffset*meta.nrows;
}

// returns a cropped rectangle from a raw DEM file
// includes edge overflow protection
// rect defined by (x,y):top left corner and width, height
int16_t* cropDEMWithMeta(string directory, string filename, demMeta meta, unsigned int x, unsigned int y, unsigned int width, unsigned int height){
    
    string path = directory + filename + ".DEM";
    FILE *file = fopen(path.c_str(), "r");
    int16_t *crop = (int16_t*)malloc(sizeof(int16_t)*width*height);
    
    unsigned long startByte = x*2 + y*2*meta.ncols;   // (*2) convert byte to uint16
    uint16_t elevation[width];
    int16_t swapped[width];
    fseek(file, startByte, SEEK_SET);
    fread(elevation, sizeof(uint16_t), width, file);
    
//    fseek(file, startByte, SEEK_SET);  //method 2
    for(int h = 0; h < height; h++){
        fseek(file, startByte+meta.ncols*2*h, SEEK_SET);  // method 2 comment this out
        fread(elevation, sizeof(uint16_t), width, file);
        // convert from little endian
        for(int i = 0; i < width; i++){
            swapped[i] = (elevation[i]>>8) | (elevation[i]<<8);
            crop[h*width+i] = swapped[i];
        }
//        startByte += NCOLS*2;
//        for(int i = 0; i < 10; i++)
//            printf("%p  %hu  %u\n",elevation[i], elevation[i], elevation[i]);
//        for(int i = 0; i < 10; i++)
//            printf("%p  %zd  %d\n",swapped[i], swapped[i], swapped[i]);
//        fseek(file, NCOLS*2, SEEK_CUR); // method 2
    }
    fclose(file);
    return crop;
}
int16_t* cropDEM(string directory, string filename, unsigned int x, unsigned int y, unsigned int width, unsigned int height){
    demMeta meta = loadHeader(directory, filename);
    return cropDEMWithMeta(directory, filename, meta, x, y, width, height);
}

void checkBoundaries(demMeta meta, unsigned int *x, unsigned int *y, unsigned int *width, unsigned int *height){
    //if rectangle overflows past boundary, will move the rectangle and maintain width and height if possible
    //if width or height is bigger than the file's, will shorten the width/height
    if(*x > meta.ncols || *y > meta.nrows){
        printf("\nEXCEPTION: starting location lies outside data\n");
        return;
    }
    if(*width > meta.ncols){
        printf("\nWARNING: width larger than data width, shrinking width to fit\n");
        *width = meta.ncols;
        *x = 0;
    }
    else if (*x+*width > meta.ncols){
        printf("\nWARNING: boundary lies outside data, adjusting origin to fit width\n");
        *x = meta.ncols-*width;
    }
    if(*height > meta.nrows){
        printf("\nWARNING: height larger than data height, shrinking height to fit\n");
        *height = meta.nrows;
        *y = 0;
    }
    else if(*y+*height > meta.nrows){
        printf("\nWARNING: boundary lies outside data, adjusting origin to fit height\n");
        *y = meta.nrows-*height;
    }
}

float* elevationPointCloud(string directory, string filename, float latitude, float longitude, unsigned int width, unsigned int height){
    if(!width || !height)
        return NULL;
    
    // load meta data from header
    demMeta meta = loadHeader(directory, filename);

    // convert lat/lon into column/row for plate
    unsigned int row, column;
    convertLatLonToXY(meta, latitude, longitude, &column, &row);
    
    // shift center point to top left, and check boundaries
    column -= width*.5;
    row -= height*.5;
    checkBoundaries(meta, &column, &row, &width, &height);
    printf("Columns:(%d to %d)\nRows:(%d to %d)\n",column, column+height, row, row+width);

    // crop DEM and load it into memory
    int16_t *data = cropDEMWithMeta(directory, filename, meta, column, row, width, height);
    
    // empty point cloud, (x, y, z)
    float *points = (float*)malloc(sizeof(float)*width*height * 3);
    
    for(int h = 0; h < height; h++){
        for(int w = 0; w < width; w++){
            points[(h*width+w)*3+0] = (w - width*.5);         // x
            points[(h*width+w)*3+1] = (h - height*.5);        // y
            int16_t elev = data[h*width+w];
            if(elev == -9999)
                points[(h*width+w)*3+2] = 0.0f;///1000.0;    // z, convert meters to km
            else
                points[(h*width+w)*3+2] = data[h*width+w];///1000.0;    // z, convert meters to km
        }
    }
    return points;
}

// point cloud
//X:longitude Y:latitude Z:elevation
//float* elevationPointsForArea(FILE *file, float latitude, float longitude, unsigned int width, unsigned int height){
//    if(!width || !height)
//        return NULL;
//    
//    unsigned long centerByte = getByteOffset(latitude, longitude);
//    unsigned long startByte = centerByte - (width*.5 * 2) - (height*.5 * NCOLS * 2);
//    unsigned int row, column;
//    startOffset(latitude, longitude, &column, &row);
//    printf("%d : %d\n",row, column);
//    int16_t *data = cropFile(file, column, row, width, height);
//    if(data == NULL) return NULL;
//    
//    printf("%lu, %lu\n", centerByte, startByte);
//    
//    float *points = (float*)malloc(sizeof(float)*width*height * 3); // x, y, z
//    
//    for(int h = 0; h < height; h++){
//        for(int w = 0; w < width; w++){
//            points[(h*width+w)*3+0] = (w - width*.5);         // x
//            points[(h*width+w)*3+1] = (h - height*.5);        // y
//            int16_t elev = data[h*width+w];
//            if(elev == -9999)
//                points[(h*width+w)*3+2] = 0.0f;///1000.0;    // z, convert meters to km
//            else
//                points[(h*width+w)*3+2] = data[h*width+w];///1000.0;    // z, convert meters to km
//        }
//    }
//    return points;
//}


