
#include "Scene.hpp"
#include <iostream>

Scene::~Scene(){
    for (int i=0;i<sceneObjects.size();i++)
    {
        delete sceneObjects[i];
    }
    sceneObjects.clear();
}


// Scene.cpp
void Scene::render(Camera* camera) {
    for (std::size_t i = 0; i < sceneObjects.size(); ++i) {
        if (sceneObjects[i] == nullptr) {
            std::cerr << "ERROR: Null object at index " << i << std::endl;
            continue;
        }

        Shader* shader = sceneObjects[i]->getShader();
        if (shader == nullptr) {
            std::cerr << "ERROR: Null shader for object " << i << std::endl;
            continue;
        }

        shader->bind();
        sceneObjects[i]->render(camera);
    }
}

void Scene::addObject(Object *object){
    sceneObjects.push_back(object);
    
}

