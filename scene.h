/*==============================================================================

Å@ âÊñ ëJà⁄êßå‰ [scene.h]
                                                         Author : Youhei Sato
                                                         Date   : 2025/07/10
--------------------------------------------------------------------------------

==============================================================================*/
#ifndef SCENE_H
#define SCENE_H


void Scene_Initialize();
void Scene_Finalize();
void Scene_Update(double elapsed_time);
void Scene_Draw();
void Scene_Refresh();

enum Scene
{
    SCENE_TITLE,
    SCENE_GAME,
    SCENE_RESULT,
    SCENE_OVER
};

void Scene_Change(Scene scene);

bool Scene_IsPaused();

#endif // SCENE_H