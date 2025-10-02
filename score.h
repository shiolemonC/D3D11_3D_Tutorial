/*==============================================================================

　 スコア管理 [score.h]
                                                         Author : Youhei Sato
                                                         Date   : 2025/07/09
--------------------------------------------------------------------------------

==============================================================================*/
#ifndef SCORE_H
#define SCORE_H

void Score_Initialize(float x, float y, int digit); // 他にゼロ埋め、左寄せなど
void Score_Finalize();
void Score_Update();
void Score_Draw();

unsigned int Score_GetScore();
void Score_AddScore(int score);
void Score_Reset();

#endif // SCORE_H
