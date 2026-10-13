package com.xingyuan.xyazl

import android.Manifest
import android.app.Activity
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.view.View
import android.widget.Button
import android.widget.ProgressBar
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.core.content.FileProvider
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File

class MainActivity : AppCompatActivity() {

    private lateinit var tvConsole: TextView
    private lateinit var btnStart: Button
    private var selectedApkUri: Uri? = null

    private val logLines = mutableListOf<String>()
    private val maxLogLines = 50

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        val btnSelect = findViewById<Button>(R.id.btn_select_apk)
        val tvPath = findViewById<TextView>(R.id.tv_apk_path)
        btnStart = findViewById(R.id.btn_start_mod)
        tvConsole = findViewById(R.id.tv_console)

        tvConsole.setTextIsSelectable(true)
        tvConsole.setOnLongClickListener {
            val clipboard = getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
            val clip = ClipData.newPlainText("ModMaker_Log", tvConsole.text.toString())
            clipboard.setPrimaryClip(clip)
            Toast.makeText(this, "日志已复制到剪贴板", Toast.LENGTH_SHORT).show()
            true
        }

        btnSelect.setOnClickListener {
            val intent = Intent(Intent.ACTION_GET_CONTENT).apply {
                type = "application/vnd.android.package-archive"
                addCategory(Intent.CATEGORY_OPENABLE)
            }
            startActivityForResult(intent, 1001)
        }

        btnStart.setOnClickListener {
            if (selectedApkUri == null) {
                Toast.makeText(this, "请先选择原始 APK", Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            startModding()
        }

        checkStoragePermission()
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode == 1001 && resultCode == Activity.RESULT_OK) {
            selectedApkUri = data?.data
            findViewById<TextView>(R.id.tv_apk_path).text = selectedApkUri?.lastPathSegment ?: "未知文件"
            btnStart.isEnabled = true
            log("已选择 APK: ${selectedApkUri?.lastPathSegment}")
            log("请确保删除原始 APP 后开始注入代理")
        }
    }

    private fun checkStoragePermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            val readPermission = ContextCompat.checkSelfPermission(this, Manifest.permission.READ_EXTERNAL_STORAGE)
            val writePermission = ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE)
            if (readPermission != PackageManager.PERMISSION_GRANTED || writePermission != PackageManager.PERMISSION_GRANTED) {
                log("请求存储读写权限...")
                ActivityCompat.requestPermissions(
                    this,
                    arrayOf(Manifest.permission.READ_EXTERNAL_STORAGE, Manifest.permission.WRITE_EXTERNAL_STORAGE),
                    1002
                )
            } else {
                log("存储权限已授予")
            }
        }
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == 1002) {
            if (grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                log("存储权限已授予，可以开始操作")
            } else {
                log("[X] 权限被拒绝，无法正常执行操作")
            }
        }
    }

    private fun startModding() {
        val progressBar = findViewById<ProgressBar>(R.id.progress_bar)
        progressBar.visibility = View.VISIBLE
        btnStart.isEnabled = false

        logLines.clear()
        tvConsole.text = ""

        lifecycleScope.launch {
            log("开始处理...")
            withContext(Dispatchers.IO) {
                val workspace = File(cacheDir, "mod_workspace")
                if (workspace.exists()) {
                    workspace.deleteRecursively()
                    withContext(Dispatchers.Main) {
                        log("[*] 已清理旧缓存目录: ${workspace.absolutePath}")
                    }
                }
            }

            val signedApk = ModEngine.performSurgery(
                this@MainActivity,
                selectedApkUri!!
            ) { msg -> log(msg) }

            if (signedApk != null) {
                log("[*] APK 生成完毕，启动安装界面...")
                installApk(signedApk)
            } else {
                log("[X] 操作失败，请检查日志")
                progressBar.visibility = View.INVISIBLE
                btnStart.isEnabled = true
            }
        }
    }

    private fun installApk(apkFile: File) {
        try {
            // 通过 FileProvider 生成 content URI
            val apkUri = FileProvider.getUriForFile(
                this,
                "${applicationContext.packageName}.fileprovider",
                apkFile
            )
            val intent = Intent(Intent.ACTION_VIEW).apply {
                setDataAndType(apkUri, "application/vnd.android.package-archive")
                addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            }
            startActivity(intent)
            findViewById<ProgressBar>(R.id.progress_bar).visibility = View.INVISIBLE
            btnStart.isEnabled = true

        } catch (e: Exception) {
            log("[X] 无法启动安装: ${e.message}")
            btnStart.isEnabled = true
            findViewById<ProgressBar>(R.id.progress_bar).visibility = View.INVISIBLE
            lifecycleScope.launch(Dispatchers.IO) {
                File(cacheDir, "mod_workspace").deleteRecursively()
            }
        }
    }

    private fun log(message: String) {
        runOnUiThread {
            logLines.add(message)
            if (logLines.size > maxLogLines) {
                logLines.removeAt(0)
            }
            tvConsole.text = logLines.joinToString("\n")
        }
    }
}