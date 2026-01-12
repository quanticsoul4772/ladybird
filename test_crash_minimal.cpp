/*
 * Minimal test to isolate TestMalwareML crash
 */

#include <Services/Sentinel/MalwareML.h>
#include <AK/ByteBuffer.h>
#include <AK/StringView.h>
#include <LibMain/Main.h>
#include <stdio.h>

using namespace Sentinel;

ErrorOr<int> ladybird_main(Main::Arguments)
{
    printf("Step 1: Creating ByteBuffer...\n");
    fflush(stdout);

    ByteBuffer benign_data;

    printf("Step 2: First append...\n");
    fflush(stdout);

    benign_data.append("Hello World! This is a test file.\n"sv.bytes());

    printf("Step 3: Second append...\n");
    fflush(stdout);

    benign_data.append("It contains normal text data.\n"sv.bytes());

    printf("Step 4: Third append...\n");
    fflush(stdout);

    benign_data.append("No suspicious content here.\n"sv.bytes());

    printf("Step 5: Creating ML detector...\n");
    fflush(stdout);

    auto detector = MalwareMLDetector::create("/tmp/dummy_model.tflite");
    if (detector.is_error()) {
        printf("ERROR: Failed to create detector: %s\n", detector.error().string_literal().characters_without_null_termination());
        return 1;
    }

    printf("Step 6: Detector created successfully\n");
    fflush(stdout);

    printf("Step 7: Extracting features...\n");
    fflush(stdout);

    auto features = detector.value()->extract_features(benign_data);
    if (features.is_error()) {
        printf("ERROR: Failed to extract features: %s\n", features.error().string_literal().characters_without_null_termination());
        return 1;
    }

    printf("Step 8: Features extracted successfully\n");
    printf("  File size: %lu bytes\n", features.value().file_size);
    printf("  Entropy: %.2f\n", features.value().entropy);
    fflush(stdout);

    printf("SUCCESS: All steps completed\n");
    return 0;
}
