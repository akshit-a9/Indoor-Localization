package com.example.demo

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.net.wifi.ScanResult
import android.net.wifi.WifiManager
import android.os.Bundle
import android.view.View
import android.widget.*
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject
import java.nio.FloatBuffer
import java.util.Locale
import ai.onnxruntime.OrtEnvironment
import ai.onnxruntime.OrtSession
import ai.onnxruntime.OnnxTensor

class MainActivity : AppCompatActivity() {

    private lateinit var modelSpinner: Spinner
    private lateinit var accuracyTextView: TextView
    private lateinit var scanButton: Button
    private lateinit var locationTextView: TextView
    private lateinit var apCountTextView: TextView
    private lateinit var statusTextView: TextView

    private val ortEnv: OrtEnvironment = OrtEnvironment.getEnvironment()
    private var ortSession: OrtSession? = null

    private var modelsRegistry = mutableMapOf<String, ModelInfo>()
    private var featureBssids = listOf<String>()
    private var labelMap = mapOf<Int, String>()

    data class ModelInfo(val name: String, val file: String, val accuracy: Double)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        modelSpinner = findViewById(R.id.modelSpinner)
        accuracyTextView = findViewById(R.id.accuracyTextView)
        scanButton = findViewById(R.id.scanButton)
        locationTextView = findViewById(R.id.locationTextView)
        apCountTextView = findViewById(R.id.apCountTextView)
        statusTextView = findViewById(R.id.statusTextView)

        loadAssetsAndSetupUI()

        scanButton.setOnClickListener {
            checkPermissionAndScan()
        }
    }

    private fun loadAssetsAndSetupUI() {
        lifecycleScope.launch(Dispatchers.IO) {
            try {
                val registryStr = assets.open("models/models_registry.json").bufferedReader().use { it.readText() }
                val registryJson = JSONObject(registryStr)
                val names = registryJson.keys()
                while (names.hasNext()) {
                    val name = names.next()
                    val obj = registryJson.getJSONObject(name)
                    modelsRegistry[name] = ModelInfo(
                        name,
                        obj.getString("onnx_file"),
                        obj.getDouble("test_accuracy")
                    )
                }

                val bssidsStr = assets.open("models/feature_bssids.json").bufferedReader().use { it.readText() }
                val bssidsArray = JSONArray(bssidsStr)
                val bssidsList = mutableListOf<String>()
                for (i in 0 until bssidsArray.length()) {
                    bssidsList.add(bssidsArray.getString(i).lowercase(Locale.US))
                }
                featureBssids = bssidsList

                val labelsStr = assets.open("models/label_map.json").bufferedReader().use { it.readText() }
                val labelsJson = JSONObject(labelsStr)
                val labelsMap = mutableMapOf<Int, String>()
                val keys = labelsJson.keys()
                while (keys.hasNext()) {
                    val key = keys.next()
                    labelsMap[key.toInt()] = labelsJson.getString(key)
                }
                labelMap = labelsMap

                withContext(Dispatchers.Main) {
                    setupSpinner()
                    apCountTextView.text = String.format(Locale.US, "APs detected: -- / %d", featureBssids.size)
                }
            } catch (e: Exception) {
                withContext(Dispatchers.Main) {
                    statusTextView.text = String.format(Locale.US, "Error loading assets: %s", e.message)
                }
            }
        }
    }

    private fun setupSpinner() {
        val sortedModels = modelsRegistry.values.sortedByDescending { it.accuracy }
        val modelNames = sortedModels.map { it.name }
        val adapter = ArrayAdapter(this, android.R.layout.simple_spinner_item, modelNames)
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        modelSpinner.adapter = adapter

        modelSpinner.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(parent: AdapterView<*>?, view: View?, position: Int, id: Long) {
                val selectedModelName = modelNames[position]
                val modelInfo = modelsRegistry[selectedModelName]!!
                accuracyTextView.text = String.format(Locale.US, "Test accuracy: %.2f%%", modelInfo.accuracy * 100)
                loadModel(modelInfo.file)
            }

            override fun onNothingSelected(parent: AdapterView<*>?) {}
        }
    }

    private fun loadModel(fileName: String) {
        lifecycleScope.launch(Dispatchers.IO) {
            try {
                ortSession?.close()
                val modelBytes = assets.open("models/$fileName").readBytes()
                ortSession = ortEnv.createSession(modelBytes, OrtSession.SessionOptions())
                withContext(Dispatchers.Main) {
                    statusTextView.text = String.format(Locale.US, "Status: Model %s loaded", fileName)
                }
            } catch (e: Exception) {
                withContext(Dispatchers.Main) {
                    statusTextView.text = String.format(Locale.US, "Error loading model: %s", e.message)
                }
            }
        }
    }

    private fun checkPermissionAndScan() {
        val permissions = arrayOf(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION)
        val missingPermissions = permissions.filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }

        if (missingPermissions.isNotEmpty()) {
            ActivityCompat.requestPermissions(this, missingPermissions.toTypedArray(), 1001)
        } else {
            performWifiScan()
        }
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == 1001) {
            if (grantResults.isNotEmpty() && grantResults.all { it == PackageManager.PERMISSION_GRANTED }) {
                performWifiScan()
            } else {
                statusTextView.text = "Status: Location permission denied"
            }
        }
    }

    private fun performWifiScan() {
        scanButton.isEnabled = false
        statusTextView.text = "Status: Scanning..."

        val wifiManager = applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager
        
        lifecycleScope.launch(Dispatchers.IO) {
            try {
                val scanResults = wifiManager.scanResults
                processScanResults(scanResults)
            } catch (e: Exception) {
                withContext(Dispatchers.Main) {
                    statusTextView.text = String.format(Locale.US, "Error scanning: %s", e.message)
                    scanButton.isEnabled = true
                }
            }
        }
    }

    private suspend fun processScanResults(scanResults: List<ScanResult>) {
        val bssidToRssi = scanResults.associate { it.BSSID.lowercase(Locale.US) to it.level.toFloat() }
        val featureCount = featureBssids.size
        val featureVector = FloatArray(featureCount) { i ->
            bssidToRssi[featureBssids[i]] ?: -100.0f
        }

        val detectedCount = scanResults.count { it.BSSID.lowercase(Locale.US) in featureBssids }

        runInference(featureVector, detectedCount)
    }

    private suspend fun runInference(featureVector: FloatArray, detectedCount: Int) {
        val session = ortSession
        if (session == null) {
            withContext(Dispatchers.Main) {
                statusTextView.text = "Status: Model not loaded"
                scanButton.isEnabled = true
            }
            return
        }

        try {
            val featureCount = featureBssids.size.toLong()
            val inputTensor = OnnxTensor.createTensor(ortEnv, FloatBuffer.wrap(featureVector), longArrayOf(1, featureCount))
            val results = session.run(mapOf("rssi_input" to inputTensor))
            
            val predictedClass = try {
                val labelArray = results.get(0).value as LongArray
                labelArray[0].toInt()
            } finally {
                results.close()
            }
            
            val locationName = labelMap[predictedClass] ?: "Unknown ($predictedClass)"

            withContext(Dispatchers.Main) {
                locationTextView.text = String.format(Locale.US, "📍 %s", locationName)
                apCountTextView.text = String.format(Locale.US, "APs detected: %d / %d", detectedCount, featureBssids.size)
                statusTextView.text = "Status: Ready"
                scanButton.isEnabled = true
            }
        } catch (e: Exception) {
            withContext(Dispatchers.Main) {
                statusTextView.text = String.format(Locale.US, "Inference error: %s", e.message)
                scanButton.isEnabled = true
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        ortSession?.close()
        ortEnv.close()
    }
}
