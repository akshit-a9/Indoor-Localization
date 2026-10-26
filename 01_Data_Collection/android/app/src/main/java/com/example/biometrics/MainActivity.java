package com.example.biometrics;  // Ensure this matches your app's package name

import android.content.Intent;
import android.os.Bundle;
import androidx.annotation.NonNull;
import io.flutter.embedding.android.FlutterActivity;
import io.flutter.plugin.common.MethodCall;
import io.flutter.plugin.common.MethodChannel;

public class MainActivity extends FlutterActivity {
    // This must match the channel name in your Flutter app
    private static final String CHANNEL = "com.example.biometrics/background";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Registering the MethodChannel to communicate between Flutter and native Android
        new MethodChannel(getFlutterEngine().getDartExecutor().getBinaryMessenger(), CHANNEL)
                .setMethodCallHandler(new MethodChannel.MethodCallHandler() {
                    @Override
                    public void onMethodCall(MethodCall call, MethodChannel.Result result) {
                        // Handle the method call
                        if (call.method.equals("startService")) {
                            // Call method to start the background service
                            startBackgroundService();
                            result.success("Service Started");
                        } else {
                            result.notImplemented();
                        }
                    }
                });
    }

    // Method to start the background service
    private void startBackgroundService() {
        Intent intent = new Intent(this, MyBackgroundService.class);
        startService(intent);  // Start the background service
    }
}
