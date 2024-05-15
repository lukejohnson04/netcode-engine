
struct RandomState {
    std::mt19937 randomEngine;
    std::uniform_real_distribution<float> floatDistribution;
    std::uniform_real_distribution<double> doubleDistribution;
};

global_variable RandomState *random_engine_state;

namespace Random {
    internal
    void Init() {
        random_engine_state->randomEngine.seed(std::random_device()());
        random_engine_state->floatDistribution = std::uniform_real_distribution<float>(0.0f, 1.0f);
        random_engine_state->doubleDistribution = std::uniform_real_distribution<double>(0.0, 1.0);
    }

    internal
    inline float Float() {
        return random_engine_state->floatDistribution(random_engine_state->randomEngine);
    }

    internal
    inline float Float(float min, float max) {
        return (Random::Float() * (max - min)) + min;
    }

    // Returns a random double in the range [0, 1)
    internal
    inline double Double() {
        return random_engine_state->doubleDistribution(random_engine_state->randomEngine);
    }

    // Returns a random double in the range [min, max)
    internal
    inline double Double(double min, double max) {
        return (Random::Double() * (max - min)) + min;
    }
}
