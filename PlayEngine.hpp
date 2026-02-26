#ifndef PLAYENGINE_HPP
#define PLAYENGINE_HPP

#include <vector>
#include <string>
#include <SDL2/SDL.h>
#include "CommonTypes.hpp"
#include "BmsonLoader.hpp"
#include "SoundManager.hpp"
#include "ChartProjector.hpp" // 追加
#include "JudgeManager.hpp"   // 追加

// 注意: PlayStatus の定義が CommonTypes.hpp にある場合、
// そこに std::vector<float> gaugeHistory; を追加する必要があります。

class PlayEngine {
public:
    /**
     * @brief 楽曲データをもとにエンジンを初期化します。
     * ゲージ의初期値設定やランダム等のオプション適用もここで行います。
     */
    void init(const BMSData& data);

    /**
     * @brief 毎フレームの更新処理。
     * BGMの再生管理や見逃しPOORの判定を行います。
     */
    void update(double cur_ms, uint32_t now, SoundManager& snd);

    /**
     * @brief プレイヤーの入力を処理し、判定を行います。
     * @return int 判定の種類 (0:None/Bad, 1:Good, 2:Great, 3:P-Great)
     */
    int processHit(int lane, double cur_ms, uint32_t now, SoundManager& snd);

    /**
     * @brief 【追加】キーを離した時の処理を行います (LN終端判定用)。
     */
    void processRelease(int lane, double cur_ms, uint32_t now);

    /**
     * @brief 強制的に失敗状態（閉店）にします。
     */
    void forceFail();

    // --- 時間・座標変換ヘルパー ---
    // 実装は内部で projector に委譲しますが、外部インターフェースは維持します
    double getMsFromY(int64_t target_y) const;
    int64_t getYFromMs(double cur_ms) const;
    double getBpmFromMs(double cur_ms) const;

    // --- データアクセス ---
    PlayStatus& getStatus() { return status; }
    const std::vector<PlayableNote>& getNotes() const { return notes; }
    const std::vector<PlayableLine>& getBeatLines() const { return beatLines; }
    JudgmentDisplay& getCurrentJudge() { return currentJudge; }

private:
    // updateGauge は JudgeManager へ移動したため、PlayEngine 内の private 関数としては削除

    BMSData bmsData;
    std::vector<PlayableNote> notes;
    std::vector<PlayableLine> beatLines;
    PlayStatus status;
    JudgmentDisplay currentJudge;

    // 分割したロジックの所有
    ChartProjector projector; // 追加
    JudgeManager judgeManager; // 追加

    /**
     * @brief 1ノーツあたりの基本回復量 (NORMALゲージ用)。
     * 楽曲のTOTAL値と総ノーツ数から算出されます。
     */
    double baseRecoveryPerNote = 0.0;

    /**
     * @brief 【修正済】処理済みの音符位置を記憶するインデックス。
     * これにより毎フレームの全件ループを回避し、Switch上での負荷を軽減します。
     */
    size_t nextUpdateIndex = 0;

    /**
     * @brief 【追加】空打ち音（オートキーサウンド）用に各レーンの最新音源名を保持します。
     * lane 1-8 に対応。
     */
    std::string lastSoundPerLane[9];

    /**
     * @brief 【追加】ゲージ推移グラフの記録用タイマー。
     */
    double lastHistoryUpdateMs = -1000.0;
};

#endif