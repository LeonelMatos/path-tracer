struct RenderConfig {
    int depth = 5;
    int samples_per_pixel = 1;
    int rr_min_bounces = 3;
    float rr_max_survival = 0.75;

    float CAM_APERTURE = 0.00;
    float CAM_FOCAL_DISTANCE = 4.7;

    bool FOCAL_DEBUG = false;
    float FOCAL_BAND_DEBUG = 0.05;
};