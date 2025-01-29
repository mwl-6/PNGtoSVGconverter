#include <iostream>
#include <string>
#include <map>
#include <unordered_map>
#include <vector>
#include <cmath>
#include <algorithm>
#include "raylib.h"
#include "raymath.h"

using namespace std;

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
    int length;
    Color color;
    vector<Coordinate> pixels;
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
    int hashWidth = 5;
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

    //Modify the image colors to match the reduced palette.

    for(int i = 0; i < image.width; i++){
        for(int j = 0; j < image.height; j++){
            Color col = GetImageColor(image, i, j);
            Color select = getClosestPaletteColor(col, recordedColors);
            ImageDrawPixel(&image, i, j, select);
        }
    }
    
}

bool colorEqual(Color a, Color b){
    return a.r == b.r && a.g == b.g && a.b == b.b && (a.a == b.a || a.a == 0 || b.a == 0);
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
            

            
            //exit(0);
            //Flood fill the region to detect its borders and clear the region from future generation
            vector<Coordinate> unexplored;
            unordered_map<string, bool> explored;
            unexplored.push_back({i, j});

            int q = 0;
            while(unexplored.size() > 0){
                q++;
                Coordinate curr = unexplored[0];

                if(q % 10000 == 0){
                    //cout << q << endl;
                    ///cout << unexplored.size() << endl;
                }
                
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
                if(i < srcImage.height-1 && j < srcImage.height-1 && colorEqual(GetImageColor(refinedImage, i+1, j+1), col)){
                    Coordinate c;
                    c.x = i+1;
                    c.y = j+1;
                    if(!explored[coordToString(c)]){
                        unexplored.push_back({i+1, j+1});
                    }
                }
                else {
                    isBorderPixel = true;
                }
                if(i > 0 && j < srcImage.height-1 && colorEqual(GetImageColor(refinedImage, i-1, j+1), col)){
                    Coordinate c;
                    c.x = i-1;
                    c.y = j+1;
                    if(!explored[coordToString(c)]){
                        unexplored.push_back({i-1, j+1});
                    }
                }
                else {
                    isBorderPixel = true;
                }
                if(i < srcImage.height-1 && j > 0 && colorEqual(GetImageColor(refinedImage, i+1, j-1), col)){
                    Coordinate c;
                    c.x = i+1;
                    c.y = j-1;
                    if(!explored[coordToString(c)]){
                        unexplored.push_back({i+1, j-1});
                    }
                }
                else {
                    isBorderPixel = true;
                }

                if(i > 0 && j > 0 && colorEqual(GetImageColor(refinedImage, i-1, j-1), col)){
                    Coordinate c;
                    c.x = i-1;
                    c.y = j-1;
                    if(!explored[coordToString(c)]){
                        unexplored.push_back({i-1, j-1});
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
            if(q > 4)
                cout << "Created region of color: " << +r->color.r << ", " << +r->color.g << ", " << +r->color.b << ", " << +r->color.a << "size: " << q << "; " << i << ", " << j << endl;
            regions.push_back(r);
        }
    }

    cout << "Generated " << regions.size() << " regions" << endl;
    for(int i = 0; i < regions.size(); i++){
        Color c = regions[i]->color;
        //int x = 0;
        for(auto it = regions[i]->unmatchedPixels.begin(); it != regions[i]->unmatchedPixels.end(); it++){
            Coordinate a = stringToCoord(it->first);
            //cout << a.x << "," << a.y << endl;
            ImageDrawPixel(&refinedImage, a.x, a.y, c);
            //x++;
        }
        //cout << i <<","<<x << endl;
    }


    
    for(int i = 0; i < regions.size(); i++){
        Region *r = regions[i];
        cout << "Generating loops for region of color: " << +r->color.r << ", " << +r->color.g << ", " << +r->color.b << ", " << +r->color.a << endl;

        
        //Now left with a cluster of unsorted pixels, they must be sorted into loops
        

        while(r->unmatchedPixels.size() > 0){
            
            Loop *loop = new Loop();

            Coordinate curr = stringToCoord(r->unmatchedPixels.begin()->first);
            Coordinate nxt = curr;
            Color currCol = GetImageColor(refinedImage, curr.x, curr.y);
            r->unmatchedPixels.erase(coordToString(curr));

            loop->color = currCol;
            loop->pixels.push_back(curr);

            bool start = false;
            while(!(curr.x == nxt.x && curr.y == nxt.y) || !start){
                start = true;
                if(nxt.y - 1 >= 0 && colorEqual(currCol, GetImageColor(refinedImage, nxt.x, nxt.y-1)) && r->unmatchedPixels[coordToString({nxt.x, nxt.y-1})]){
                    nxt.y--;
                    r->unmatchedPixels.erase(coordToString(nxt));
                }
                else if(nxt.x - 1 >= 0 && colorEqual(currCol, GetImageColor(refinedImage, nxt.x-1, nxt.y)) && r->unmatchedPixels[coordToString({nxt.x-1, nxt.y})]){
                    nxt.x--;
                    r->unmatchedPixels.erase(coordToString(nxt));
                }
                else if(nxt.y + 1 < refinedImage.height && colorEqual(currCol, GetImageColor(refinedImage, nxt.x, nxt.y+1)) && r->unmatchedPixels[coordToString({nxt.x, nxt.y+1})]){
                    nxt.y++;
                    r->unmatchedPixels.erase(coordToString(nxt));
                }
                else if(nxt.x + 1  < refinedImage.width && colorEqual(currCol, GetImageColor(refinedImage, nxt.x+1, nxt.y)) && r->unmatchedPixels[coordToString({nxt.x+1, nxt.y})]){
                    nxt.x++;
                    r->unmatchedPixels.erase(coordToString(nxt));
                }
                
                else if(nxt.x + 1 < refinedImage.width && nxt.y + 1 < refinedImage.height && colorEqual(currCol, GetImageColor(refinedImage, nxt.x+1, nxt.y+1)) && r->unmatchedPixels[coordToString({nxt.x+1, nxt.y+1})]){
                    nxt.x++;
                    nxt.y++;
                    r->unmatchedPixels.erase(coordToString(nxt));
                }
                else if(nxt.x - 1 >= 0 && nxt.y + 1 < refinedImage.height && colorEqual(currCol, GetImageColor(refinedImage, nxt.x-1, nxt.y+1)) && r->unmatchedPixels[coordToString({nxt.x-1, nxt.y+1})]){
                    nxt.x--;
                    nxt.y++;
                    r->unmatchedPixels.erase(coordToString(nxt));
                }
                else if(nxt.x + 1 < refinedImage.width && nxt.y - 1 >= 0 && colorEqual(currCol, GetImageColor(refinedImage, nxt.x+1, nxt.y-1)) && r->unmatchedPixels[coordToString({nxt.x+1, nxt.y-1})]){
                    nxt.x++;
                    nxt.y--;
                    r->unmatchedPixels.erase(coordToString(nxt));
                }
                else if(nxt.x - 1 >= 0 && nxt.y - 1 >= 0 && colorEqual(currCol, GetImageColor(refinedImage, nxt.x-1, nxt.y-1)) && r->unmatchedPixels[coordToString({nxt.x-1, nxt.y-1})]){
                    nxt.x--;
                    nxt.y--;
                    r->unmatchedPixels.erase(coordToString(nxt));
                }
                

                loop->pixels.push_back(nxt);
            }
            loop->pixels.erase(loop->pixels.begin() + loop->pixels.size()-1);
            loop->length = loop->pixels.size();
            
            r->loops.push_back(loop);
            cout << "Added new loop of length: " << loop->length << endl;
        }
    }
    

    cout << "Defined " << regions.size() << " regions" << endl;
    

}

int main(){

    int screenWidth = 1280;
    int screenHeight = 720;

    string filePath;
    int colorSize;

    cout << "Please enter an image file: " << endl;
    getline(cin, filePath);
    cout << "Please enter the color palette size: " << endl;
    cin >> colorSize;

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
                completedSteps++;
                
            }
            else if(completedSteps == 2){
                

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
        
        
        if(completedSteps >= 1){
            DrawTextureEx(filteredTexture, {stepWidth, screenHeight - imgHeight*scale}, 0, scale, WHITE);

            float w = (float)screenWidth / (float)colorSize;
            //cout << w << endl;
            
            for(int i = 0; i < colorSize; i++){
                DrawRectangle(i*(int)w, 160, (int)w, min((int)w, 100), {(unsigned char)recordedColors[i].r, (unsigned char)recordedColors[i].g, (unsigned char)recordedColors[i].b, (unsigned char)recordedColors[i].a});
            }
        }
        if(completedSteps >= 2){
            DrawTextureEx(refinedTexture, {stepWidth*2, screenHeight - imgHeight*scale}, 0, scale, WHITE);
        }
        if(completedSteps >= 3){
            DrawTextureEx(filteredTexture, {stepWidth*3, screenHeight - imgHeight*scale}, 0, scale, WHITE);
        }
        DrawLine(0, screenHeight-imgHeight*scale, screenWidth, screenHeight-imgHeight*scale, BLACK);
        DrawLine(0, screenHeight, screenWidth, screenHeight, BLACK);

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
        
	
		EndDrawing();
	}
    return 0;
}