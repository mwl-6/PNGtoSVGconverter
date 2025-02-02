#include <iostream>
#include <string>
#include <map>
#include <unordered_map>
#include <vector>
#include <cmath>
#include <fstream>
#include <algorithm>
#include "raylib.h"
#include "raymath.h"

using namespace std;

Color nullColor;
float polygonError = 5.0f;
int hashWidth = 10;
bool smoothEdges = false;

typedef struct ColorRecord {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
    int count;
} ColorRecord;


typedef struct Coordinate {
    int x;
    int y;
} Coordinate;

//A loop is a border of pixels between two colors
typedef struct Loop {
    bool closed;
    int length;
    int idealLength;
    float idealError;
    float area;
    Color color;
    vector<Coordinate> pixels;
    vector<Coordinate> simplifiedShape;

} Loop;

//A region is a space of like-color pixels that may contain several loops
//The largest loop will become the basis of the shape defining the region while the
//smaller loops will be used in later regions
typedef struct Region {
    Color color;
    unordered_map<string, bool> unmatchedPixels;
    vector<Loop*> loops;
} Region;

string colorToString(Color c){
    return to_string(c.r).append(",").append(to_string(c.g)).append(",").append(to_string(c.b)).append(",").append(to_string(c.a));
}

bool compColor(const ColorRecord &a, const ColorRecord &b) {
    return a.count > b.count;
}

float ColorDistance(Color a, Color b){
    return sqrtf((a.r - b.r)*(a.r - b.r) + (a.g - b.g)*(a.g - b.g) + (a.b - b.b)*(a.b - b.b) + (a.a - b.a)*(a.a - b.a));
}

Color getClosestPaletteColor(Color col, vector<ColorRecord> &palette){
    Color closest = col;
    float closestDist = 999999;
    for(int i = 0; i < palette.size(); i++){
        
        float dist = ColorDistance(col, {palette[i].r, palette[i].g, palette[i].b, palette[i].a});
        if(dist < closestDist){
            closestDist = dist;
            closest.r = palette[i].r;
            closest.g = palette[i].g;
            closest.b = palette[i].b;
            closest.a = palette[i].a;
        }
    }
    
    return closest;
}

typedef struct SpatialKey {
    int index;
    int bucket;
} SpatialKey;


int hashCoords(int x, int y, int z, int w) {
  const int hash = (x * 92837111) ^ (y * 689287499) ^ (z * 283923481) ^ (w * 392018394);
  return abs(hash);
}
//Used when sorting spatial hash array
int compareKeys(const void *a, const void *b){
  int l = ((struct SpatialKey*)a)->bucket;
  int r = ((struct SpatialKey*)b)->bucket;
  return l-r;
}

bool colorEqual(Color a, Color b){
    return a.r == b.r && a.g == b.g && a.b == b.b && (a.a == b.a || a.a == 0 || b.a == 0);
}

void findNullColor(vector<ColorRecord> &colors){
    bool foundColor = false;
    while(!foundColor){
        nullColor.r = (unsigned char)(rand() % 255);
        nullColor.g = (unsigned char)(rand() % 255);
        nullColor.b = (unsigned char)(rand() % 255);
        nullColor.a = 255;
        foundColor = true;
        for(int i = 0; i < colors.size(); i++){
            if(colorEqual(nullColor, {colors[i].r, colors[i].g, colors[i].b, colors[i].a})){
                foundColor = false;
                break;
            }
        }
    }
}

void reduceColors(Image &image, int numColors, unordered_map<string, int> &colorData, vector<ColorRecord> &recordedColors){
    //Get color map
    for(int i = 0; i < image.width; i++){
        for(int j = 0; j < image.height; j++){
            Color col = GetImageColor(image, i, j);
            
            string c = colorToString(col);
            if(colorData.find(c) != colorData.end()){
                colorData[c]++;
            }
            else {
                colorData[c] = 1;
            }
        }
    }

    //
    cout << "Total colors: " << colorData.size() << endl;


    for(auto it = colorData.begin(); it != colorData.end(); it++){
        int firstComma = it->first.find(",");
        int secondComma = it->first.find(",", firstComma+1);
        int thirdComma = it->first.find(",", secondComma+1);
        
        int cR = stoi(it->first.substr(0, firstComma));
        int cG = stoi(it->first.substr(firstComma+1, secondComma-firstComma-1));
        int cB = stoi(it->first.substr(secondComma+1, thirdComma-secondComma-1));
        int cA = stoi(it->first.substr(thirdComma+1));
        
        ColorRecord c;
        c.r = cR;
        c.g = cG;
        c.b = cB;
        c.a = cA;
        c.count = it->second;
        recordedColors.push_back(c);
    }

    std::sort(recordedColors.begin(), recordedColors.end(), compColor);

    /*
    for(int i = 0; i < 10; i++){
        cout << recordedColors[i].r << ", " << recordedColors[i].g << ", " << recordedColors[i].b << ": " << recordedColors[i].count << endl;
    }
    */

    //Grouping alike colors using spatial hashing
    
    int hashTableSize = recordedColors.size() * 5;

    //Records the index, bucket of each recorded color    
    SpatialKey *keyTable = new SpatialKey[recordedColors.size()];

    //Groups together like elements in similar buckets
    int *startingIndexTable = new int[hashTableSize];
    for(int i = 0; i < hashTableSize; i++){
        startingIndexTable[i] = -1;
    }

    //Assign a bucket to an element
    for(int i = 0; i < recordedColors.size(); i++){
        int x = floor(recordedColors[i].r / hashWidth);
        int y = floor(recordedColors[i].g / hashWidth);
        int z = floor(recordedColors[i].b / hashWidth);
        int w = floor(recordedColors[i].a / hashWidth);

        int bucket = hashCoords(x, y, z, w) % hashTableSize;
        keyTable[i].bucket = bucket;
        keyTable[i].index = i;
    }

    //Sort the colors by bucket so they are next to each other
    qsort((void*) keyTable, recordedColors.size(), sizeof(SpatialKey), compareKeys);


    //Mark the starting index of each bucket
    for(int i = 0; i < recordedColors.size(); i++){
        int currentBucket = keyTable[i].bucket;
        if(startingIndexTable[currentBucket] > i || startingIndexTable[currentBucket] == -1){
            startingIndexTable[currentBucket] = i;
        }
    }


    /*
    Merging process: Using the spatial hash, identify like colors and merge them into one entry
    Adjust colorData accordingly
    */
    

    for(int i = 0; i < recordedColors.size(); i++){
        int x = recordedColors[i].r;
        int y = recordedColors[i].g;
        int z = recordedColors[i].b;
        int w = recordedColors[i].a;

        int bx = floor(x / hashWidth);
        int by = floor(y / hashWidth);
        int bz = floor(z / hashWidth);
        int bw = floor(w / hashWidth);

        int bucket = hashCoords(bx, by, bz, bw) % hashTableSize;
        int startingIndex = startingIndexTable[bucket];

        //Of the group of similar colors, which is the most common
        Color dominantColor;
        dominantColor.r = x;
        dominantColor.g = y;
        dominantColor.b = z;
        dominantColor.a = w;
        //How many pixels for all alike colors
        int totalAlike = recordedColors[i].count;
        //
        bool merge = false;
        //
        int dominantIndex = i;
        int dominantCount = recordedColors[i].count;

        while(startingIndex != -1 && startingIndex < recordedColors.size() && keyTable[startingIndex].bucket == bucket){
            int colorIndex = keyTable[startingIndex].index;
            //Check if there is a valid color nearby
            if(ColorDistance({(unsigned char)x, (unsigned char)y, (unsigned char)z, (unsigned char)w}, {recordedColors[colorIndex].r, recordedColors[colorIndex].g, recordedColors[colorIndex].b, recordedColors[colorIndex].a}) < hashWidth && i != colorIndex && recordedColors[colorIndex].count > 0){
                merge = true;
                //If this color is more common record so
                if(recordedColors[colorIndex].count > recordedColors[dominantIndex].count){
                    dominantColor.r = recordedColors[colorIndex].r;
                    dominantColor.g = recordedColors[colorIndex].g;
                    dominantColor.b = recordedColors[colorIndex].b;
                    dominantColor.a = recordedColors[colorIndex].a;
                    dominantIndex = colorIndex;
                    totalAlike += recordedColors[colorIndex].count;
                    dominantCount = recordedColors[colorIndex].count;
                }
                else {
                    totalAlike += recordedColors[colorIndex].count;
                    recordedColors[colorIndex].count = 0;
                }
            }

            startingIndex++;
        }

        if(merge){
            if(dominantIndex == i){
                recordedColors[i].count = totalAlike;
            }
            else {
                recordedColors[dominantIndex].count = totalAlike;
                recordedColors[i].count = 0;
            }
        }

    }

    int i = recordedColors.size()-1;
    int merged = 0;
    while(i >= 0 && recordedColors.size() > 0){
        if(recordedColors[i].count == 0){
            recordedColors.erase(recordedColors.begin() + i);
            merged++;
        }
        i--;
    }

    cout << "Merged " << merged << " colors, total colors: " << recordedColors.size() << endl;
    
    std::sort(recordedColors.begin(), recordedColors.end(), compColor);
    
    if(recordedColors.size() > numColors){
        recordedColors.erase(recordedColors.begin() + numColors, recordedColors.end());
    }

    cout << "Reduced to top " << recordedColors.size() << " colors" << endl;

    /*
    for(int i = 0; i < recordedColors.size(); i++){
        if(recordedColors[i].r == 0 && recordedColors[i].g == 0 && recordedColors[i].b == 0 && recordedColors[i].a > 0){
            recordedColors[i].r = 1;
        }
    }
    */
    //Modify the image colors to match the reduced palette.
    findNullColor(recordedColors);


    for(int i = 0; i < image.width; i++){
        for(int j = 0; j < image.height; j++){
            Color col = GetImageColor(image, i, j);
            Color select = getClosestPaletteColor(col, recordedColors);
            if(col.r == 0 && col.g == 0 && col.b == 0 && col.a == 0){
                ImageDrawPixel(&image, i, j, nullColor);
            }
            else{
                ImageDrawPixel(&image, i, j, select);
            }
        }
    }
    
}



string coordToString(Coordinate c){
    return to_string(c.x).append(",").append(to_string(c.y));
}
Coordinate stringToCoord(string c){
    int split = c.find(",");
    int x = stoi(c.substr(0, split));
    int y = stoi(c.substr(split+1));
    return {x, y};
}

void refineBorders(Image &srcImage, Image &refinedImage, vector<Region*> &regions){
    Color erased;
    erased.r = 255;
    erased.g = 255;
    erased.b = 255;
    erased.a = 0;


    for(int i = 0; i < srcImage.width; i++){
        for(int j = 0; j < srcImage.height; j++){
            Color col = GetImageColor(refinedImage, i, j);
            if(col.a == 0){
                continue;
            }
            //Otherwise, create a new region to explore
            Region *r = new Region();
            
            r->color = col;
            

            //Flood fill the region to detect its borders and clear the region from future generation
            vector<Coordinate> unexplored;
            unordered_map<string, bool> explored;
            unexplored.push_back({i, j});

            int q = 0;
            while(unexplored.size() > 0){
                q++;
                Coordinate curr = unexplored[0];

                
                unexplored.erase(unexplored.begin());
                explored[coordToString(curr)] = true;
                //explored[curr] = true;
                //Automatically mark as an edge
                bool isBorderPixel = false;
                if(curr.x == 0 || curr.x == srcImage.width-1 || curr.y == 0 || curr.y == srcImage.height-1){
                    isBorderPixel = true;
                }

                /*
                2 purposes:
                1. Find adjacent pixels that are part of the region
                2. Detect if the current pixel is an edge (borders another color)
                */
                
                if(curr.x > 0 && colorEqual(GetImageColor(refinedImage, curr.x-1, curr.y), col)){
                    Coordinate c;
                    c.x = curr.x-1;
                    c.y = curr.y;
                    if(!explored[coordToString(c)]){
                        unexplored.push_back({curr.x-1, curr.y});
                        explored[coordToString(c)] = true;
                    }
                }
                else {
                    isBorderPixel = true;
                }
                if(curr.x < srcImage.width-1 && colorEqual(GetImageColor(refinedImage, curr.x+1, curr.y), col)){
                    Coordinate c;
                    c.x = curr.x+1;
                    c.y = curr.y;
                    if(!explored[coordToString(c)]){
                        unexplored.push_back({curr.x+1, curr.y});
                        explored[coordToString(c)] = true;
                    }
                }
                else {
                    isBorderPixel = true;
                }
                if(curr.y > 0 && colorEqual(GetImageColor(refinedImage, curr.x, curr.y-1), col)){
                    Coordinate c;
                    c.x = curr.x;
                    c.y = curr.y-1;
                    if(!explored[coordToString(c)]){
                        unexplored.push_back({curr.x, curr.y-1});
                        explored[coordToString(c)] = true;
                    }
                }
                else {
                    isBorderPixel = true;
                }
                if(curr.y < srcImage.height-1 && colorEqual(GetImageColor(refinedImage, curr.x, curr.y+1), col)){
                    Coordinate c;
                    c.x = curr.x;
                    c.y = curr.y+1;
                    if(!explored[coordToString(c)]){
                        unexplored.push_back({curr.x, curr.y+1});
                        explored[coordToString(c)] = true;
                    }
                }
                else {
                    isBorderPixel = true;
                }
                /*
                if(curr.x < srcImage.height-1 && curr.y < srcImage.height-1 && colorEqual(GetImageColor(refinedImage, curr.x+1, curr.y+1), col)){
                    Coordinate c;
                    c.x = curr.x+1;
                    c.y = curr.y+1;
                    if(!explored[coordToString(c)]){
                        unexplored.push_back({curr.x+1, curr.y+1});
                    }
                }
                else {
                    isBorderPixel = true;
                }
                if(curr.x > 0 && curr.y < srcImage.height-1 && colorEqual(GetImageColor(refinedImage, curr.x-1, curr.y+1), col)){
                    Coordinate c;
                    c.x = curr.x-1;
                    c.y = curr.y+1;
                    if(!explored[coordToString(c)]){
                        unexplored.push_back({curr.x-1, curr.y+1});
                    }
                }
                else {
                    isBorderPixel = true;
                }
                if(curr.x < srcImage.height-1 && curr.y > 0 && colorEqual(GetImageColor(refinedImage, curr.x+1, curr.y-1), col)){
                    Coordinate c;
                    c.x = curr.x+1;
                    c.y = curr.y-1;
                    if(!explored[coordToString(c)]){
                        unexplored.push_back({curr.x+1, curr.y-1});
                    }
                }
                else {
                    isBorderPixel = true;
                }

                if(curr.x > 0 && curr.y > 0 && colorEqual(GetImageColor(refinedImage, curr.x-1, curr.y-1), col)){
                    Coordinate c;
                    c.x = curr.x-1;
                    c.y = curr.y-1;
                    if(!explored[coordToString(c)]){
                        unexplored.push_back({curr.x-1, curr.y-1});
                    }
                }
                else {
                    isBorderPixel = true;
                }
                */
                
                
                if(isBorderPixel){
                    r->unmatchedPixels[coordToString(curr)] = true;
                    ImageDrawPixel(&refinedImage, curr.x, curr.y, {col.r, col.g, col.b, 0});
                }
                else {
                    //Mark current pixel as clear
                    ImageDrawPixel(&refinedImage, curr.x, curr.y, {col.r, col.g, col.b, 0});
                }
            }
            /*
            if(q > 4)
                cout << "Created region of color: " << +r->color.r << ", " << +r->color.g << ", " << +r->color.b << ", " << +r->color.a << "size: " << q << "; " << i << ", " << j << endl;
            */
            regions.push_back(r);
        }
    }

    
    for(int i = regions.size()-1; i >= 0; i--){
        Color c = regions[i]->color;
        
        
        //Removing irrelevant regions
        if(regions[i]->unmatchedPixels.size() < 10){
            regions.erase(regions.begin() + i);
            continue;
        }
        
        
        for(auto it = regions[i]->unmatchedPixels.begin(); it != regions[i]->unmatchedPixels.end(); it++){
            Coordinate a = stringToCoord(it->first);
            
            ImageDrawPixel(&refinedImage, a.x, a.y, c);
            
        }
        
        
    }
    cout << "Generated " << regions.size() << " regions" << endl;


    for(int i = 0; i < regions.size(); i++){
        Region *r = regions[i];
        //cout << "Generating loops for region of color: " << +r->color.r << ", " << +r->color.g << ", " << +r->color.b << ", " << +r->color.a << ", size: " << r->unmatchedPixels.size() << endl;

        
        //Now left with a cluster of unsorted pixels, they must be sorted into loops
        

        while(r->unmatchedPixels.size() > 0){
            
            Coordinate curr = stringToCoord(r->unmatchedPixels.begin()->first);
            Coordinate nxt = curr;
            Color currCol = GetImageColor(refinedImage, curr.x, curr.y);
            
            r->unmatchedPixels.erase(r->unmatchedPixels.begin()->first);

            if(currCol.a == 0){
                continue;
            }
            
            Loop *loop = new Loop();
            loop->color = currCol;
            loop->pixels.push_back(curr);

            bool start = false;
            int loopSize = 0;
            bool closeLoop = false;

            while((!(curr.x == nxt.x && curr.y == nxt.y) || !start) && !closeLoop){
                loopSize++;
                start = true;


                
                if(nxt.y - 1 >= 0 && r->unmatchedPixels.count(coordToString({nxt.x, nxt.y-1})) == 1){
                    nxt.y--;
                    r->unmatchedPixels.erase(coordToString(nxt));
                }
                else if(nxt.x - 1 >= 0 && r->unmatchedPixels.count(coordToString({nxt.x-1, nxt.y})) == 1){
                    nxt.x--;
                    r->unmatchedPixels.erase(coordToString(nxt));
                }
                else if(nxt.y + 1 < refinedImage.height && r->unmatchedPixels.count(coordToString({nxt.x, nxt.y+1})) == 1){
                    nxt.y++;
                    r->unmatchedPixels.erase(coordToString(nxt));
                }
                else if(nxt.x + 1  < refinedImage.width && r->unmatchedPixels.count(coordToString({nxt.x+1, nxt.y})) == 1){
                    nxt.x++;
                    r->unmatchedPixels.erase(coordToString(nxt));
                }
                
                else if(nxt.x + 1 < refinedImage.width && nxt.y + 1 < refinedImage.height && r->unmatchedPixels.count(coordToString({nxt.x+1, nxt.y+1})) == 1){
                    nxt.x++;
                    nxt.y++;
                    r->unmatchedPixels.erase(coordToString(nxt));
                }
                else if(nxt.x - 1 >= 0 && nxt.y + 1 < refinedImage.height && r->unmatchedPixels.count(coordToString({nxt.x-1, nxt.y+1})) == 1){
                    nxt.x--;
                    nxt.y++;
                    r->unmatchedPixels.erase(coordToString(nxt));
                }
                else if(nxt.x + 1 < refinedImage.width && nxt.y - 1 >= 0 && r->unmatchedPixels.count(coordToString({nxt.x+1, nxt.y-1})) == 1){
                    nxt.x++;
                    nxt.y--;
                    r->unmatchedPixels.erase(coordToString(nxt));
                }
                else if(nxt.x - 1 >= 0 && nxt.y - 1 >= 0 && r->unmatchedPixels.count(coordToString({nxt.x-1, nxt.y-1})) == 1){
                    nxt.x--;
                    nxt.y--;
                    r->unmatchedPixels.erase(coordToString(nxt));
                }
                else if(abs(nxt.x - curr.x) <= 1 && abs(nxt.y - curr.y) <= 1){
                    nxt.x = curr.x;
                    nxt.y = curr.y;
                }
                else {

                    /*There are some edge cases where a border may branch off in a way that requires
                    backtracking to continue. By default, this isn't possible as the pixels leading back to the
                    next path have already been covered.

                    To address this, perform a search for the nearest pixel and jump to it
                    */
                    float shortestDist = 9999;
                    Coordinate shortestI;
                    shortestI.x = -1;
                    shortestI.y = -1;
                    
                    for(auto it = r->unmatchedPixels.begin(); it != r->unmatchedPixels.end(); it++){
                        Coordinate t = stringToCoord(it->first);
                        float d = Vector2Distance({(float)nxt.x, (float)nxt.y}, {(float)t.x, (float)t.y});
                        if(d < shortestDist){
                            shortestDist = d;
                            shortestI.x = t.x;
                            shortestI.y = t.y;
                        }
                    }
                    if(shortestI.x >= 0){

                        
                        if(Vector2Distance({(float)nxt.x, (float)nxt.y}, {(float)curr.x, (float)curr.y}) < shortestDist){
                            nxt.x = curr.x;
                            nxt.y = curr.y;
                        }
                        else{
                            nxt.x = shortestI.x;
                            nxt.y = shortestI.y;
                            r->unmatchedPixels.erase(coordToString(nxt));
                        }
                        
                    }
                    else if(r->unmatchedPixels.size() == 0){
                        nxt.x = curr.x;
                        nxt.y = curr.y;
                    }
                    else {
                        //cout << "No options for " << +r->color.r << ", " << +r->color.g << ", " << +r->color.b << endl;
                        //No other options, end the loop
                        closeLoop = true;
                    }

                    
                }
                
                loop->pixels.push_back(nxt);

                if(loopSize > srcImage.width * srcImage.height){
                    closeLoop = true;
                    cout << "Too large, close loop" << endl;
                }
            }
            //loop->pixels.erase(loop->pixels.begin() + loop->pixels.size()-1);
            loop->length = loop->pixels.size();
            loop->closed = !closeLoop;
            
            r->loops.push_back(loop);
            //cout << "Added new loop of length: " << loop->length << "; closeLoop = " << closeLoop << endl;
        }
        //cout << "Region " << i << ": " << r->loops.size() << " loops" << endl;
    }
    

    //cout << "Defined " << regions.size() << " regions" << endl;

    /*
    Now clear all inside loops for each region
    */
    for(int i = 0; i < regions.size(); i++){
        int largestLoopIndex = 0;
        int largestSize = -1;
        if(regions[i]->loops.size() == 1){
            continue;
        }
        for(int j = 0; j < regions[i]->loops.size(); j++){
            if(regions[i]->loops[j]->length > largestSize){
                largestSize = regions[i]->loops[j]->length;
                largestLoopIndex = j;
            }
        }
        Loop *tmp = regions[i]->loops[0];
        regions[i]->loops[0] = regions[i]->loops[largestLoopIndex];
        regions[i]->loops[largestLoopIndex] = tmp;
        for(int j = 1; j < regions[i]->loops.size(); j++){
            delete regions[i]->loops[j];
        }
        regions[i]->loops.erase(regions[i]->loops.begin() + 1, regions[i]->loops.end());
    }

    cout << "Erased extraneous loops" << endl;

    /*
    for(int i = 0; i < regions.size(); i++){
        cout << regions[i]->loops.size() << ", " << regions[i]->loops[0]->length << endl;
    }
    */
    
    

}

float polygonLength(vector<Coordinate> &pixels){
    float len = 0;
    for(int i = 0; i < pixels.size(); i++){
        int j = (i+1)%pixels.size();
        len += Vector2Distance({(float)pixels[i].x, (float)pixels[i].y}, {(float)pixels[j].x, (float)pixels[j].y});
    }
    return len;
}
//Shoelace Algorithm
float calculateArea(vector<Coordinate> &pixels){
    if(pixels.size() == 1){
        return 1;
    }
    float l = 0;
    float r = 0;
    for(int i = 0; i < pixels.size(); i++){
        int j = (i+1) % pixels.size();
        l += pixels[i].x * pixels[j].y;
        r += pixels[i].y * pixels[j].x;
    }
    
    
    float area = abs(l - r) * 0.5f;
    return area;
}

float distToLine(Vector2 p1, Vector2 p2, Vector2 p3){
  float d = Vector2Distance(p1, p2);
  
  float u = ((p3.x - p1.x)*(p2.x - p1.x) + (p3.y - p1.y)*(p2.y - p1.y)) / (d*d);
  
  
  float cx = p1.x + u * (p2.x - p1.x);
  float cy = p1.y + u * (p2.y - p1.y);
  
  if(u < 0 || u > 1){
    float d1 = Vector2Distance({p3.x, p3.y}, {p1.x, p1.y});
    float d2 = Vector2Distance({p3.x, p3.y}, {p2.x, p2.y});
    if(d1 < d2){
      cx = p1.x;
      cy = p1.y;
    }
    else {
      cx = p2.x;
      cy = p2.y;
    }
  }
  float distance = Vector2Distance({p3.x, p3.y}, {cx, cy});
  return distance;
}

float visvalingam(vector<Coordinate> &pixels, int count, float originalLength, vector<Coordinate> &reference){

    //Perform visvalingam algorithm
    for(int n = 0; n < count; n++){
        int len = pixels.size();
        float minArea = 999999;
        int minIndex = -1;
        for(int i = 0; i < len; i++){
            int j = (i+1) % len;
            int k = (j+1) % len;
            //(1/2) |x1(y2 − y3) + x2(y3 − y1) + x3(y1 − y2)|
            Coordinate p1 = pixels[i];
            Coordinate p2 = pixels[j];
            Coordinate p3 = pixels[k];
            float area = 0.5f * abs(p1.x * (p2.y - p3.y) + p2.x * (p3.y - p1.y) + p3.x * (p1.y - p2.y));
            if(area < minArea){
                minArea = area;
                minIndex = j;
            }
        }
        if(minIndex > -1){
            pixels.erase(pixels.begin() + minIndex);
        }
    }

    //Calculate error:

    float maxDist = 0;
    for(int i = 0; i < reference.size(); i++){
        float minDist = -1;
        int minIndex = -1;
        Coordinate p = reference[i];
        
        for(int c = 0; c < pixels.size(); c++){
            int d = (c+1)%pixels.size();

            float distance = distToLine({(float)pixels[c].x, (float)pixels[c].y}, {(float)pixels[d].x, (float)pixels[d].y}, {(float)p.x, (float)p.y});
            
            if(distance < minDist || minIndex == -1){
                minDist = distance;
                minIndex = c;
            }
            
        }
            
        /*
        if(minDist > maxDist && minDist < 99999){
            maxDist = minDist;
        }
        */
       maxDist += minDist;
        
    }
    if(pixels.size() == 1){
        maxDist = 9999999 * reference.size();
    }

    //float newLength = polygonLength(pixels);
    //float newLength = calculateArea(pixels);
    
    float error = maxDist / reference.size();
    
    return error;
}

void generatePolygons(Image &image, vector<Region*> &regions){

    int totalVertices = 0;
    int reducedVertices = 0;

    for(int i = 0; i < image.width; i++){
        for(int j = 0; j < image.height; j++){
            ImageDrawPixel(&image, i, j, WHITE);
        }
    }

    for(int i = 0; i < regions.size(); i++){
        for(int j = 0; j < regions[i]->loops.size(); j++){

            for(int k = 0; k < regions[i]->loops[j]->pixels.size(); k++){
                ImageDrawPixel(&image, regions[i]->loops[j]->pixels[k].x, regions[i]->loops[j]->pixels[k].y, PURPLE);
            }
        }
    }

    for(int i = 0; i < regions.size(); i++){
        
        Loop *loop = regions[i]->loops[0];
        loop->idealLength = loop->length;
        float originalLength = polygonLength(loop->pixels);
        float originalArea = calculateArea(loop->pixels);
        //cout << "area: " << originalArea << endl;
        float localPolygonError = polygonError;
        
        //Uncomment for no simplification:
        //loop->simplifiedShape.clear();
        //copy(loop->pixels.begin(), loop->pixels.end(), back_inserter(loop->simplifiedShape));
        
        for(int j = loop->length - 1; j >= 0; j--){
            
            loop->simplifiedShape.clear();
            copy(loop->pixels.begin(), loop->pixels.end(), back_inserter(loop->simplifiedShape));
            
            float error = visvalingam(loop->simplifiedShape, j, originalLength, loop->pixels);
            if(loop->length - j < loop->idealLength && error < localPolygonError){
                loop->idealLength = loop->length - j;
                loop->idealError = error;
                break;
            }
        }
        loop->simplifiedShape.clear();
        copy(loop->pixels.begin(), loop->pixels.end(), back_inserter(loop->simplifiedShape));
        float error = visvalingam(loop->simplifiedShape, loop->length - loop->idealLength, originalLength, loop->pixels);
        
        /*
        for(int i = 0; i < loop->simplifiedShape.size(); i++){
            cout << loop->simplifiedShape[i].x << ", " << loop->simplifiedShape[i].y << endl;
        }
        */
        
        //cout << "Reduced region " << i << " from " << loop->length << " to " << loop->idealLength << ": error: " << error << endl;
        totalVertices += loop->length;
        reducedVertices += loop->idealLength;
        
    }

    cout << "Completed polygon generation" << endl;
    cout << "Reduced scene vertices from " << totalVertices << " to " << reducedVertices << endl;
}



bool compareAreas(const Loop *a, const Loop *b){
  return a->area > b->area;
}

typedef struct Bezier {
    float x1, y1, cx1, cy1, cx2, cy2, x2, y2;
} Bezier;

/*
Creates a Centripetal Catmull-Rom Spline using 4 vertices
Then converts to a bezier format that can be rendered

https://en.wikipedia.org/wiki/Centripetal_Catmull%E2%80%93Rom_spline
Centripetal Catmull Rom Spline Implementation
  
Converting to cubic bezier
https://stackoverflow.com/questions/30748316/catmull-rom-interpolation-on-svg-paths
*/
void CalculateBezierFromCatmullRom(Bezier &bezier, int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3){
    float t[] = {0, 0, 0, 0};
    float px[] = {(float)x0, (float)x1, (float)x2, (float)x3};
    float py[] = {(float)y0, (float)y1, (float)y2, (float)y3};

    //Check for duplicates
    for(int i = 0; i < 4; i++){
        for(int j = 0; j < 4; j++){
            if(i==j)
                continue;
            if(px[i] == px[j] && py[i] == py[j]){
                bezier.x1 = x1;
                bezier.y1 = y1;
                bezier.x2 = x2;
                bezier.y2 = y2;
                bezier.cx1 = x1;
                bezier.cy1 = y1;
                bezier.cx2 = x2;
                bezier.cy2 = y2;
                return;
            }
        }
    }

    //centripetal
    float a = 0.5;
    for(int tv = 1; tv < 4; tv++){
      
      int tv2 = (tv-1);
      float dX = px[tv] - px[tv2];
      float dY = py[tv] - py[tv2];
      dX = dX*dX;
      dY = dY*dY;
      float l = sqrt(dX + dY);
      
      t[tv] = pow(l, a) + t[tv-1];
      
    }

    float c1 = (t[2] - t[1]) / (t[2] - t[0]);
    float c2 = (t[1] - t[0]) / (t[2] - t[0]);
    float d1 = (t[3]-t[2])/(t[3]-t[1]);
    float d2 = (t[2]-t[1])/(t[3]-t[1]);

    float m1[] = {0, 0};
    float m2[] = {0, 0};

    m1[0] = (t[2]-t[1])*(c1*(px[1] - px[0]) / (t[1]-t[0]) + c2 * (px[2] - px[1]) / (t[2]-t[1]));
    m1[1] = (t[2]-t[1])*(c1*(py[1] - py[0]) / (t[1]-t[0]) + c2 * (py[2] - py[1]) / (t[2]-t[1]));


    m2[0] = (t[2]-t[1])*(d1*(px[2]-px[1])/(t[2]-t[1]) + d2*(px[3]-px[2])/(t[3]-t[2]));
    m2[1] = (t[2]-t[1])*(d1*(py[2]-py[1])/(t[2]-t[1]) + d2*(py[3]-py[2])/(t[3]-t[2]));


    
    float q1[] = {px[1] + m1[0] / 3.0f, py[1] + m1[1] / 3.0f};
    float q2[] = {px[2] - m2[0] / 3.0f, py[2] - m2[1] / 3.0f};

    bezier.x1 = x1;
    bezier.y1 = y1;
    bezier.x2 = x2;
    bezier.y2 = y2;
    bezier.cx1 = q1[0];
    bezier.cy1 = q1[1];
    bezier.cx2 = q2[0];
    bezier.cy2 = q2[1];


}

const float cornerThreshold = 122;

bool treatPointAsCorner(int x0, int y0, int x1, int y1, int x2, int y2){
    //reverse a
    Vector2 a = {(float)x0 - x1, (float)y0 - y1};
    Vector2 b = {(float)x2 - x1, (float)y2 - y1};
    float d = Vector2DotProduct(a, b);
    float distA = Vector2Length(a);
    float distB = Vector2Length(b);
    float angle = acosf(d / (distA * distB));
    float angleDegrees = angle * 180.0f / PI;

    return angleDegrees < cornerThreshold;

}


void writeToFile(string path, vector<Region*> &regions, Image &reference){
    cout << "Writing to " << path << endl;
    vector<Loop *> loops;
    for(int i = regions.size()-1; i >= 0; i--){
        Loop *loop = regions[i]->loops[0];
        
        loop->area = calculateArea(loop->simplifiedShape);
        //cout << loop->pixels.size() << ", " << loop->area << endl;
        
        loops.push_back(loop);
        

    }
    cout << "Calculated areas" << endl;

    std::sort(loops.begin(), loops.end(), compareAreas);

    //Avoid treating clear background as a rectangle mask
    int backgroundIndex = -1;
    int backgroundSize = -1;
    for(int i = 0; i < loops.size(); i++){
        if(colorEqual(loops[i]->color, nullColor) && loops[i]->area > backgroundSize){
            backgroundIndex = i;
            backgroundSize = loops[i]->area;
        }
    }

    //Final round of refinement (remove anymore extraneous vertices)

    float cullingThreshold = 5;
    int removeCount = 0;
    for(int k = 0; k < 1; k++){
        bool removed = false;
        for(int i = 0; i < loops.size(); i++){
            //visvalingam(loops[i]->simplifiedShape, loops[i]->simplifiedShape.size()/2, loops[i]->simplifiedShape.size());
            for(int j = loops[i]->simplifiedShape.size()-2; j >= 0; j--){

                int l = j+1;
                
                int m = j-1;
                if(m == -1){
                    m = loops[i]->simplifiedShape.size()-1;
                }

                float d1 = Vector2Distance({(float)loops[i]->simplifiedShape[l].x, (float)loops[i]->simplifiedShape[l].y}, {(float)loops[i]->simplifiedShape[j].x, (float)loops[i]->simplifiedShape[j].y} );
                float d2 = Vector2Distance({(float)loops[i]->simplifiedShape[j].x, (float)loops[i]->simplifiedShape[j].y}, {(float)loops[i]->simplifiedShape[m].x, (float)loops[i]->simplifiedShape[m].y} );
                float d3 = Vector2Distance({(float)loops[i]->simplifiedShape[l].x, (float)loops[i]->simplifiedShape[l].y}, {(float)loops[i]->simplifiedShape[m].x, (float)loops[i]->simplifiedShape[m].y} );
                //cout << d1 << "+" << d2 << " = " << d3 << "... " << (d3/(d1+d2))<< endl;
                if(d2+d1 < cullingThreshold){
                    //loops[i]->simplifiedShape.erase(loops[i]->simplifiedShape.begin() + j);
                    //removed = true;
                    //removeCount++;
                }

            }
        }
    }
    cout << "Cleared " << removeCount << " extra vertices" << endl;
    //cout << "Sorted by area" << endl;
    
    ofstream userFile(path);
    if(userFile.is_open()){
        userFile.clear();
        userFile << "<svg width=\"" << reference.width << "\" height = \"" << reference.height << "\" xmlns=\"http://www.w3.org/2000/svg\">" << endl;
        
        //Create Masks
        userFile << "<defs>\n<mask id=\"sceneMask\">" << endl;
        userFile << "<rect width=\"" << reference.width << "\" height=\"" << reference.height << "\" fill=\"white\" />" << endl; 
        for(int i = 0; i < loops.size(); i++){
            bool curve = false;
            if(loops[i]->idealLength > 5 && smoothEdges){
                curve = true;
            }
            if(!colorEqual(loops[i]->color, nullColor) || (i == backgroundIndex)){
                continue;
            }
            userFile << "<path d=\"M ";
            userFile << loops[i]->simplifiedShape[0].x << " " << loops[i]->simplifiedShape[0].y << " ";
            
            int start = curve ? 1 : 0;
            for(int j = start; j < loops[i]->idealLength; j+=1){
                int h = (j-1)%loops[i]->idealLength;
                if(j == 0)
                    h = loops[i]->idealLength-1;
                int k = (j+1)%loops[i]->idealLength;
                int l = (k+1)%loops[i]->idealLength;
                
                bool sharpCorner = treatPointAsCorner(loops[i]->simplifiedShape[h].x, loops[i]->simplifiedShape[h].y,
                loops[i]->simplifiedShape[j].x, loops[i]->simplifiedShape[j].y,
                loops[i]->simplifiedShape[k].x, loops[i]->simplifiedShape[k].y);

                sharpCorner = sharpCorner | treatPointAsCorner(loops[i]->simplifiedShape[j].x, loops[i]->simplifiedShape[j].y,
                loops[i]->simplifiedShape[k].x, loops[i]->simplifiedShape[k].y,
                loops[i]->simplifiedShape[l].x, loops[i]->simplifiedShape[l].y);

                Bezier b;
                CalculateBezierFromCatmullRom(b, loops[i]->simplifiedShape[h].x, loops[i]->simplifiedShape[h].y,
                loops[i]->simplifiedShape[j].x, loops[i]->simplifiedShape[j].y,
                loops[i]->simplifiedShape[k].x, loops[i]->simplifiedShape[k].y,
                loops[i]->simplifiedShape[l].x, loops[i]->simplifiedShape[l].y);

                if(curve && !sharpCorner){
                    userFile << "C ";
                    userFile << b.cx1 << " " << b.cy1 << " ";
                    userFile << b.cx2 << " " << b.cy2 << " ";
                    userFile << b.x2 << " " << b.y2;
                }
                else if(sharpCorner){
                    userFile << "L ";
                    userFile << b.x1 << " " << b.y1;
                    //userFile << b.x2 << " " << b.y2;
                }
                else {
                    userFile << b.x2 << " " << b.y2;
                }
                
                if(j != loops[i]->idealLength-1){
                    userFile << " ";
                }
            }
            userFile << "\" fill=\"black\" />" << endl;

        }
        userFile << "</mask>\n</defs>" << endl;

        //Add Polylines
        for(int i = 0; i < loops.size(); i++){
            bool curve = false;
            if(loops[i]->idealLength > 5 && smoothEdges){
                curve = true;
            }
            if(colorEqual(loops[i]->color, nullColor)){
                continue;
            }
            userFile << "<path d=\"M ";
            userFile << loops[i]->simplifiedShape[0].x << " " << loops[i]->simplifiedShape[0].y << " ";
            
            int start = curve ? 1 : 0;
            for(int j = start; j < loops[i]->idealLength; j+=1){
                int h = (j-1)%loops[i]->idealLength;
                if(j == 0)
                    h = loops[i]->idealLength-1;
                int k = (j+1)%loops[i]->idealLength;
                int l = (k+1)%loops[i]->idealLength;
                
                bool sharpCorner = treatPointAsCorner(loops[i]->simplifiedShape[h].x, loops[i]->simplifiedShape[h].y,
                loops[i]->simplifiedShape[j].x, loops[i]->simplifiedShape[j].y,
                loops[i]->simplifiedShape[k].x, loops[i]->simplifiedShape[k].y);

                sharpCorner = sharpCorner | treatPointAsCorner(loops[i]->simplifiedShape[j].x, loops[i]->simplifiedShape[j].y,
                loops[i]->simplifiedShape[k].x, loops[i]->simplifiedShape[k].y,
                loops[i]->simplifiedShape[l].x, loops[i]->simplifiedShape[l].y);

                Bezier b;
                CalculateBezierFromCatmullRom(b, loops[i]->simplifiedShape[h].x, loops[i]->simplifiedShape[h].y,
                loops[i]->simplifiedShape[j].x, loops[i]->simplifiedShape[j].y,
                loops[i]->simplifiedShape[k].x, loops[i]->simplifiedShape[k].y,
                loops[i]->simplifiedShape[l].x, loops[i]->simplifiedShape[l].y);

                if(curve && !sharpCorner){
                    userFile << "C ";
                    userFile << b.cx1 << " " << b.cy1 << " ";
                    userFile << b.cx2 << " " << b.cy2 << " ";
                    userFile << b.x2 << " " << b.y2;
                }
                else if(sharpCorner){
                    userFile << "L ";
                    userFile << b.x1 << " " << b.y1;
                    //userFile << b.x2 << " " << b.y2;
                }
                else {
                    userFile << "L " << b.x1 << " " << b.y1;
                }
                
                if(j != loops[i]->idealLength-1){
                    userFile << " ";
                }
            }
            userFile << "\" fill=\"rgb(" << +loops[i]->color.r << "," << +loops[i]->color.g << "," << +loops[i]->color.b << ")\"";
            userFile << " fill-opacity=\"" << (int)((+loops[i]->color.a) / (255.0f) * 100.0f) << "%\" mask=\"url(#sceneMask)\"/>" << endl;
        
        }
        
        userFile << "</svg>" << endl;
        
        cout << "Wrote to file: " << path << endl;
        userFile.close();

        
    }
    else {
        cout << "Failed to write to file " << path << endl;
    }

}

int main(int argc, char *argv[]){

    /*
    Arguments:
    Image Path
    Output Path
    Number of colors
    % Error Allowed
    Display Interactive Visualization? (true/false)

    */

    int screenWidth = 1280;
    int screenHeight = 720;

    string filePath;
    string outputPath;
    int colorSize;
    bool interaction = false;

    if(argc < 4){
        cout << "Not enough arguments!" << endl;
        exit(0);
    }
    if(argc > 7){
        cout << "Too many arguments!" << endl;
        exit(0);
    }

    if(argc >= 4){
        filePath = argv[1];
        cout << "Image path: " << filePath << endl;
        outputPath = argv[2];
        cout << "Output path: " << outputPath << endl;
        
        colorSize = stoi(argv[3]);
        cout << "# of Colors: " << colorSize << endl;
    }
    if(argc >= 7){
        polygonError = stof(argv[4]);
        cout << "Polygon % error: " << polygonError << endl;

        if(string(argv[5]) == "true"){
            interaction = true;
            cout << "Opening interactive display" << endl;
        }
        else {
            cout << "No display" << endl;
        }
        if(string(argv[6]) == "true"){
            smoothEdges = true;
        }
        else {
            smoothEdges = false;
        }
    }
    else {
        cout << "No polygon % error specified. Default to 5%" << endl;
        cout << "No display chosen. Default to command line" << endl;
    }
    

    //cout << "Please enter an image file: " << endl;
    //getline(cin, filePath);
    //cout << "Please enter the color palette size: " << endl;
    //cin >> colorSize;

	InitWindow(screenWidth, screenHeight, "Image to SVG Converter");

    //User loaded image file
    Image userImg = LoadImage(filePath.c_str());
    if(!IsImageReady(userImg)){
        cout << "Failed to load image: " << filePath << endl;
        exit(0);
    }
    ImageFormat(&userImg, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    //Image w/ reduced colors
    Image filteredImg = ImageCopy(userImg);

    //Shows the extraction of edges
    Image refinedBorders;

    //Shows the creation of shapes for SVG image based on pixel edges (with edge reduction applied)
    Image definedPolygons;
    
    Texture2D imgTexture = LoadTextureFromImage(userImg);
    Texture2D filteredTexture;
    Texture2D refinedTexture;
    Texture2D definedTexture;

    float imgWidth = filteredImg.width;
    float imgHeight = filteredImg.height;
    float stepWidth = screenWidth / 4.0f;
    float scale = stepWidth / imgWidth;

    vector<string> headers = {"Original", "Reduced Colors", "Extracted Edges", "Generate Polygons"};

    int completedSteps = 0;

    Color selected = RED;

    //A collection of every scene color and their frequency
    vector<ColorRecord> recordedColors;
    unordered_map<string, int> colorData;

    vector<Region*> regions;
    

    
	while(!WindowShouldClose()){
        
        if(IsKeyPressed(KEY_SPACE)){
            if(completedSteps == 0){
                reduceColors(filteredImg, colorSize, colorData, recordedColors);
                filteredTexture = LoadTextureFromImage(filteredImg);
                refinedBorders = ImageCopy(filteredImg);
                completedSteps++;
            }
            else if(completedSteps == 1){
                refineBorders(filteredImg, refinedBorders, regions);
                refinedTexture = LoadTextureFromImage(refinedBorders);
                definedPolygons = ImageCopy(filteredImg);
                
                completedSteps++;
                
            }
            else if(completedSteps == 2){
                generatePolygons(definedPolygons, regions);
                writeToFile(outputPath, regions, userImg);
                definedTexture = LoadTextureFromImage(definedPolygons);
                completedSteps++;
            }
        }
		
		BeginDrawing();
		
		ClearBackground(GRAY);
		
        DrawTextureEx(imgTexture, {0, screenHeight - imgHeight*scale}, 0, scale, WHITE);
        
        DrawText("Image to SVG Converter", 10, 10, 20, RED);
        DrawText(filePath.c_str(), 10, 40, 20, BLACK);
        char exportP[128];
        sprintf(exportP, "%s%s", "Export Path: ", "/test.png");
        DrawText(exportP, 10, 70, 20, BLACK);
        char step[30];
        sprintf(step, "%s%d", "Current step: ", completedSteps+1);
        DrawText(step, 10, 100, 20, BLACK);
        DrawText("Space to continue", 10, 130, 20, BLACK);        
        int sampleX = (int)(GetMouseX() / scale);
        int sampleY = (int)(( GetMouseY() - (screenHeight - imgHeight*scale)) / scale);
        

        if(sampleX >= 0 && sampleX < imgWidth && sampleY > 0 && sampleY < imgHeight){
            selected = GetImageColor(userImg, sampleX, sampleY);
        }
        DrawRectangle(screenWidth-40, 0, 40, 40, selected);
        
        //Labels for steps
        for(int i = 0; i < completedSteps+1; i++){
            DrawLine(i * stepWidth, screenHeight - imgHeight*scale, i * stepWidth, screenHeight, BLACK);
            if(i != 1){
                DrawText(headers[i].c_str(), i * stepWidth + stepWidth / 4.0f, screenHeight - imgHeight*scale - 50, 20, BLACK);
            }
            else {
                char t[30];
                sprintf(t, "%s%d%s", "Reduced Colors [", colorSize, "]");
                DrawText(t, i * stepWidth + stepWidth / 4.0f, screenHeight - imgHeight*scale - 50, 20, BLACK);
            }
        }
        DrawLine(0, screenHeight-imgHeight*scale, screenWidth, screenHeight-imgHeight*scale, BLACK);
        DrawLine(0, screenHeight, screenWidth, screenHeight, BLACK);

        if(completedSteps >= 1){
            float w = (float)screenWidth / (float)colorSize;
            //cout << w << endl;
            
            for(int i = 0; i < colorSize; i++){
                DrawRectangle(i*(int)w, 160, (int)w, min((int)w, 100), {(unsigned char)recordedColors[i].r, (unsigned char)recordedColors[i].g, (unsigned char)recordedColors[i].b, (unsigned char)recordedColors[i].a});
            }

            DrawTextureEx(filteredTexture, {stepWidth, screenHeight - imgHeight*scale}, 0, scale, WHITE);

            if(GetMouseX() > stepWidth && GetMouseX() < stepWidth*2 && GetMouseY() > screenHeight - imgHeight*scale && GetMouseY() < screenHeight){
                DrawTextureEx(filteredTexture, {0, 0}, 0, 1, WHITE);
            }
            
        }
        if(completedSteps >= 2){
            DrawTextureEx(refinedTexture, {stepWidth*2, screenHeight - imgHeight*scale}, 0, scale, WHITE);
            if(GetMouseX() > stepWidth*2 && GetMouseX() < stepWidth*3 && GetMouseY() > screenHeight - imgHeight*scale && GetMouseY() < screenHeight){
                DrawTextureEx(refinedTexture, {0, 0}, 0, 1, WHITE);
            }
        }
        if(completedSteps >= 3){
            for(int i = 0; i < regions.size(); i++){
                vector<Coordinate> s = regions[i]->loops[0]->simplifiedShape;
                if(regions[i]->loops[0]->closed){
                    for(int j = 0; j < s.size(); j++){
                        int k = (j+1)%s.size();
                        if(regions[i]->color.a != 0){
                            
                            DrawLine(s[j].x, s[j].y, s[k].x, s[k].y, {regions[i]->color});
                            DrawEllipse(s[j].x, s[j].y, 1, 1, BLACK);
                            //DrawRectangle(s[j].x, s[j].y, 1, 1, regions[i]->color);
                            
                        }
                        else {
                            
                            DrawLine(s[j].x, s[j].y, s[k].x, s[k].y, {regions[i]->color.r, regions[i]->color.g, regions[i]->color.b, 255});
                        }
                    }
                }
            }
            DrawTextureEx(definedTexture, {stepWidth*3, screenHeight - imgHeight*scale}, 0, scale, WHITE);
        }
        
        
        
	
		EndDrawing();
	}
    return 0;
}