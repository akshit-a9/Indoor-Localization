package com.example.biometrics;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.os.Build;
import android.util.Log;

public class MyBackgroundService extends Service {

    private static final String CHANNEL_ID = "background_service_channel";

    @Override
    public void onCreate() {
        super.onCreate();

        // Create the notification channel if the Android version is O or higher
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel = new NotificationChannel(
                    CHANNEL_ID,
                    "Background Service",
                    NotificationManager.IMPORTANCE_LOW
            );
            NotificationManager manager = getSystemService(NotificationManager.class);
            if (manager != null) {
                manager.createNotificationChannel(channel);
            }
        }

        // Create the notification
        Notification notification = new Notification.Builder(this, CHANNEL_ID)
                .setContentTitle("App Running")
                .setContentText("App is running in the background")
                .setSmallIcon(R.mipmap.ic_launcher)  // You can use your app's icon here
                .setPriority(Notification.PRIORITY_LOW)  // Low priority for background service notifications
                .build();

        // Start the service in the foreground with the notification
        startForeground(1, notification);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.d("MyBackgroundService", "Service started");
        // Keep the service running
        return START_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;  // No binding is needed for this service
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        Log.d("MyBackgroundService", "Service stopped");
    }
}
