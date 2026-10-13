plugins {
    id("com.android.application")
}

android {
    namespace = "com.xingyuan.xyazl"
    compileSdk {
        version = release(36) {
            minorApiLevel = 1
        }
    }

    signingConfigs {
        create("release") {
            storeFile = file("src/main/assets/xyazl.keystore")
            storePassword = "C++ is the best language in the world, and xylu is the best C++ library."
            keyAlias = "xingyuan"
            keyPassword = "C++ is the best language in the world, and xylu is the best C++ library."
        }
    }

    defaultConfig {
        applicationId = "com.xingyuan.xyazl"
        minSdk = 24
        targetSdk = 36
        versionCode = 1
        versionName = "1.0"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            signingConfig = signingConfigs.getByName("release")
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
}

configurations.all {
    exclude(group = "io.modelcontextprotocol.sdk")
    exclude(group = "tools.jackson.core")
}

dependencies {
    implementation("androidx.core:core-ktx:1.18.0")
    implementation("androidx.appcompat:appcompat:1.7.1")
    implementation("com.google.android.material:material:1.14.0")
    implementation("androidx.activity:activity:1.13.0")
    implementation("androidx.constraintlayout:constraintlayout:2.2.1")
    implementation("org.smali:dexlib2:2.5.2")
    implementation("com.google.guava:guava:33.6.0-jre")
    implementation("com.google.guava:guava:33.6.0-android")
    implementation("com.android.tools.build:apksig:9.0.1")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.10.0")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.11.0")
    testImplementation("junit:junit:4.13.2")
    androidTestImplementation("androidx.test.ext:junit:1.3.0")
    androidTestImplementation("androidx.test.espresso:espresso-core:3.7.0")
    implementation("com.madgag.spongycastle:core:1.58.0.0")
    implementation("com.madgag.spongycastle:prov:1.58.0.0")
    implementation("de.fraunhofer.sit.sse.flowdroid:soot-infoflow-android:2.15.1")
}