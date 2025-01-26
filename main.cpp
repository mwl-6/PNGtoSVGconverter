#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <cmath>
#include <algorithm>
#include "raylib.h"

using namespace std;


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

	while(!WindowShouldClose()){
		
		BeginDrawing();
		
		ClearBackground(WHITE);
		
        DrawTextureEx(imgTexture, {0, 0}, 0, scale, WHITE);
        if(completedSteps >= 1)
        DrawTextureEx(filteredTexture, {stepWidth, 0}, 0, scale, WHITE);
        if(completedSteps >= 2)
        DrawTextureEx(filteredTexture, {stepWidth*2, 0}, 0, scale, WHITE);
        if(completedSteps >= 3)
        DrawTextureEx(filteredTexture, {stepWidth*3, 0}, 0, scale, WHITE);
        DrawLine(0, 0, screenWidth, 0, BLACK);
        DrawLine(0, imgHeight*scale, screenWidth, imgHeight*scale, BLACK);
        for(int i = 0; i < completedSteps+1; i++){
            DrawLine(i * stepWidth, 0, i * stepWidth, imgHeight * scale, BLACK);
            if(i != 1){
                DrawText(headers[i].c_str(), i * stepWidth + stepWidth / 4.0f, imgHeight*scale + 20, 20, BLACK);
            }
            else {
                char t[30];
                sprintf(t, "%s%d%s", "Reduced Colors [", colorSize, "]");
                DrawText(t, i * stepWidth + stepWidth / 4.0f, imgHeight*scale + 20, 20, BLACK);
            }
        }
        
	
		EndDrawing();
	}
    return 0;
}