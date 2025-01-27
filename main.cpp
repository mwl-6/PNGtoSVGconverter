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
    int r;
    int g;
    int b;
    int count;
} ColorRecord;


float colorDist(string col1, string col2){
    int firstComma = col1.find(",");
    int secondComma = col1.find(",", firstComma+1);
    
    int cR = stoi(col1.substr(0, firstComma));
    int cG = stoi(col1.substr(firstComma+1, secondComma-firstComma-1));
    int cB = stoi(col1.substr(secondComma+1));

    int firstComma2 = col2.find(",");
    int secondComma2 = col2.find(",", firstComma2+1);
    
    int cR2 = stoi(col2.substr(0, firstComma2));
    int cG2 = stoi(col2.substr(firstComma2+1, secondComma2-firstComma2-1));
    int cB2 = stoi(col2.substr(secondComma2+1));

    return Vector3Distance({(float)cR, (float)cG, (float)cB}, {(float)cR2, (float)cG2, (float)cB2});
    
}

string colorToString(Color c){
    return to_string(c.r).append(",").append(to_string(c.g)).append(",").append(to_string(c.b));
}

bool compColor(const ColorRecord &a, const ColorRecord &b) {
    return a.count > b.count;
}

Color getClosestPaletteColor(Color col, vector<ColorRecord> &palette){
    Color closest = col;
    float closestDist = 999999;
    for(int i = 0; i < palette.size(); i++){
        float dist = Vector3Distance({(float)col.r, (float)col.g, (float)col.b}, {(float)palette[i].r, (float)palette[i].g, (float)palette[i].b});
        
        if(dist < closestDist){
            closestDist = dist;
            closest.r = palette[i].r;
            closest.g = palette[i].g;
            closest.b = palette[i].b;
        }
    }
    
    return closest;
}

typedef struct SpatialKey {
    int index;
    int bucket;
} SpatialKey;


int hashCoords(int x, int y, int z) {
  const int hash = (x * 92837111) ^ (y * 689287499) ^ (z * 283923481);
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
        
        int cR = stoi(it->first.substr(0, firstComma));
        int cG = stoi(it->first.substr(firstComma+1, secondComma-firstComma-1));
        int cB = stoi(it->first.substr(secondComma+1));
        
        ColorRecord c;
        c.r = cR;
        c.g = cG;
        c.b = cB;
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
    int hashWidth = 10;
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

        int bucket = hashCoords(x, y, z) % hashTableSize;
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

        int bx = floor(x / hashWidth);
        int by = floor(y / hashWidth);
        int bz = floor(z / hashWidth);

        int bucket = hashCoords(bx, by, bz) % hashTableSize;
        int startingIndex = startingIndexTable[bucket];

        //Of the group of similar colors, which is the most common
        Color dominantColor;
        dominantColor.r = x;
        dominantColor.g = y;
        dominantColor.b = z;
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
            if(Vector3Distance({(float)x, (float)y, (float)z}, {(float)recordedColors[colorIndex].r, (float)recordedColors[colorIndex].g, (float)recordedColors[colorIndex].b}) < hashWidth && i != colorIndex && recordedColors[colorIndex].count > 0){
                merge = true;
                //If this color is more common record so
                if(recordedColors[colorIndex].count > recordedColors[dominantIndex].count){
                    dominantColor.r = recordedColors[colorIndex].r;
                    dominantColor.g = recordedColors[colorIndex].g;
                    dominantColor.b = recordedColors[colorIndex].b;
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
    

    /*
    string greatest = "";
    int most = -1;
    for(auto it = colorData.begin(); it != colorData.end(); it++){
        if(it->second > most){
            greatest = it->first;
            most = it->second;
        }
    }
    mostData[greatest] = most;
    */
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
    //Image w/ reduced colors
    Image filteredImg = ImageCopy(userImg);

    //Shows the extraction of edges
    Image refinedBorders = ImageCopy(userImg);

    //Shows the creation of shapes for SVG image based on pixel edges (with edge reduction applied)
    Image definedPolygons = ImageCopy(userImg);
    
    Texture2D imgTexture = LoadTextureFromImage(userImg);
    Texture2D filteredTexture = LoadTextureFromImage(filteredImg);
    Texture2D refinedTexture = LoadTextureFromImage(refinedBorders);
    Texture2D definedTexture = LoadTextureFromImage(definedPolygons);

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
    

    
	while(!WindowShouldClose()){

        if(IsKeyPressed(KEY_SPACE)){
            if(completedSteps == 0){
                reduceColors(filteredImg, colorSize, colorData, recordedColors);
                filteredTexture = LoadTextureFromImage(filteredImg);
                completedSteps++;
            }
            else if(completedSteps == 1){

                completedSteps++;
            }
            else if(completedSteps == 2){
                
                completedSteps++;
            }
        }
		
		BeginDrawing();
		
		ClearBackground(WHITE);
		
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
                DrawRectangle(i*(int)w, 160, (int)w, min((int)w, 100), {(unsigned char)recordedColors[i].r, (unsigned char)recordedColors[i].g, (unsigned char)recordedColors[i].b, 255});
            }
        }
        if(completedSteps >= 2){
            DrawTextureEx(filteredTexture, {stepWidth*2, screenHeight - imgHeight*scale}, 0, scale, WHITE);
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