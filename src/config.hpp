struct RenderConfig {
    int depth = 5;
    int samples_per_pixel = 1;
    int rr_min_bounces = 3;
    float rr_max_survival = 0.75;

    int background = 0;

    int tone_mapping = 2;

    float cam_aperture = 0.00;
    float cam_focal_distance = 4.7;

    bool focal_debug = false;
    float focal_band_debug = 0.05;
};