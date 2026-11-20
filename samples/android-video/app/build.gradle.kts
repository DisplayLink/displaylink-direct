plugins {
    id("com.android.application")
}

// Root of the DisplayLink Direct bundle that provides include/dlsdk/dlsdk.h, dlsdkConfig.cmake and
// libs/android-21/<abi>/libdlsdk.so. Defaults to this repository, override with
// -Pdlsdk.root=/path/to/dlsdk.
val dlsdkRoot: String = (findProperty("dlsdk.root") as String?)
    ?: rootProject.projectDir.resolve("../..").canonicalPath

val dlsdkAndroidPlatform = "android-21"
val dlsdkAbi = "arm64-v8a"

android {
    namespace = "com.displaylink.video"
    compileSdk = 37

    defaultConfig {
        applicationId = "com.displaylink.video"
        // Minimum Android version 15 (API level 35)
        minSdk = 35
        targetSdk = 37
        versionCode = 1
        versionName = "1.0.0"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

        ndk {
            // DisplayLink Direct ships arm64-v8a binaries for Android.
            abiFilters += dlsdkAbi
        }

        externalNativeBuild {
            cmake {
                arguments += listOf(
                    "-DCMAKE_PREFIX_PATH=$dlsdkRoot",
                    "-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH",
                    "-DDLSDK_ANDROID_PLATFORM=$dlsdkAndroidPlatform",
                )
                cppFlags += "-std=c++17"
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
        }
    }

    sourceSets {
        getByName("main") {
            // libdlsdk.so and its runtime dependency libusb-1.0.so are consumed straight from the
            // DisplayLink Direct bundle. src/main/jniLibs is an optional override location.
            jniLibs.srcDirs("src/main/jniLibs", "$dlsdkRoot/libs/$dlsdkAndroidPlatform")
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_21
        targetCompatibility = JavaVersion.VERSION_21
    }

    buildFeatures {
        viewBinding = true
    }

    packaging {
        jniLibs {
            // System.loadLibrary() resolves libdlsdk.so and libusb-1.0.so straight from the APK.
            useLegacyPackaging = false
        }
    }
}

// Fail early with an actionable message instead of crashing at runtime with an UnsatisfiedLinkError.
val checkDlsdkBundle = tasks.register("checkDlsdkBundle") {
    group = "verification"
    description = "Checks that the DisplayLink Direct Android bundle and libusb are available."

    val bundleDir = file("$dlsdkRoot/libs/$dlsdkAndroidPlatform/$dlsdkAbi")
    val overrideDir = file("src/main/jniLibs/$dlsdkAbi")
    val header = file("$dlsdkRoot/include/dlsdk/dlsdk.h")

    doLast {
        check(header.isFile) {
            "DisplayLink Direct headers not found at ${header.path}. " +
                "Pass -Pdlsdk.root=/path/to/dlsdk."
        }
        check(bundleDir.resolve("libdlsdk.so").isFile) {
            "libdlsdk.so not found in ${bundleDir.path}. Download the Android release asset " +
                "into libs/$dlsdkAndroidPlatform/$dlsdkAbi/, or pass -Pdlsdk.root=/path/to/dlsdk."
        }
        check(
            bundleDir.resolve("libusb-1.0.so").isFile ||
                overrideDir.resolve("libusb-1.0.so").isFile
        ) {
            "libusb-1.0.so not found in ${bundleDir.path} or ${overrideDir.path}. libdlsdk.so " +
                "links against it, so build libusb >= 1.0.27 for $dlsdkAbi and copy it into one " +
                "of those directories. See the sample README."
        }
    }
}

tasks.matching { it.name.startsWith("merge") && it.name.endsWith("NativeLibs") }.configureEach {
    dependsOn(checkDlsdkBundle)
}

dependencies {
    implementation("androidx.core:core-ktx:1.15.0")
    implementation("androidx.appcompat:appcompat:1.7.0")
    implementation("com.google.android.material:material:1.12.0")
    implementation("androidx.constraintlayout:constraintlayout:2.2.0")
    implementation("androidx.lifecycle:lifecycle-viewmodel-ktx:2.8.7")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.8.7")
    implementation("androidx.activity:activity-ktx:1.9.3")

    // ExoPlayer decodes the MP4 straight into the SurfaceTexture that feeds the GL compositor.
    implementation("androidx.media3:media3-exoplayer:1.11.1")
    implementation("androidx.media3:media3-common:1.11.1")

    testImplementation("junit:junit:4.13.2")
}
