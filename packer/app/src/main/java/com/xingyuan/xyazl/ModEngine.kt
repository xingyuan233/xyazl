package com.xingyuan.xyazl

import android.content.Context
import android.net.Uri
import com.android.apksig.ApkSigner
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.jf.dexlib2.AccessFlags
import org.jf.dexlib2.DexFileFactory
import org.jf.dexlib2.Opcode
import org.jf.dexlib2.Opcodes
import org.jf.dexlib2.iface.ClassDef
import org.jf.dexlib2.iface.Method
import org.jf.dexlib2.iface.instruction.Instruction
import org.jf.dexlib2.immutable.ImmutableClassDef
import org.jf.dexlib2.immutable.ImmutableMethod
import org.jf.dexlib2.immutable.ImmutableMethodImplementation
import org.jf.dexlib2.immutable.instruction.ImmutableInstruction10x
import org.jf.dexlib2.immutable.instruction.ImmutableInstruction21c
import org.jf.dexlib2.immutable.instruction.ImmutableInstruction35c
import org.jf.dexlib2.immutable.reference.ImmutableMethodReference
import org.jf.dexlib2.immutable.reference.ImmutableStringReference
import org.jf.dexlib2.rewriter.DexRewriter
import org.jf.dexlib2.rewriter.Rewriter
import org.jf.dexlib2.rewriter.RewriterModule
import org.jf.dexlib2.rewriter.Rewriters
import org.jf.dexlib2.writer.io.FileDataStore
import org.jf.dexlib2.writer.pool.DexPool
import soot.jimple.infoflow.android.axml.AXmlHandler
import soot.jimple.infoflow.android.axml.AXmlNode
import java.io.*
import java.security.KeyStore
import java.security.Security
import java.util.zip.*

object ModEngine {

    suspend fun performSurgery(
        context: Context,
        apkUri: Uri,
        logCallback: (String) -> Unit
    ): File? = withContext(Dispatchers.IO) {
        try {
            val workspace = File(context.cacheDir, "mod_workspace")
            if (workspace.exists()) workspace.deleteRecursively()
            workspace.mkdirs()

            logCallback("[>] 开始处理 APK 文件...")

            // 1. 提取原始 APK
            logCallback("[1/6] 正在从 URI 提取原始 APK...")
            val baseApkFile = File(workspace, "base.apk")
            extractApkFromUri(context, apkUri, baseApkFile, logCallback)
            logCallback("[1/6] 原始 APK 提取完成 (${baseApkFile.length()} bytes)")

            // 2. 解压原始 APK
            val unzippedDir = File(workspace, "unzipped")
            unzipApk(baseApkFile, unzippedDir, logCallback)
            logCallback("[2/6] APK 解压完成")

            // 3. 植入原生库
            logCallback("[3/6] 正在植入原生库 (arm64-v8a, x86, x86_64)...")
            implantSeedLibraries(context, unzippedDir, logCallback)
            logCallback("[3/6] 原生库植入完成")

            // 4. 字节码注入
            logCallback("[4/6] 开始字节码注入...")
            injectDalvikBytecode(unzippedDir, logCallback)
            logCallback("[4/6] 字节码注入完成")

            // 5. 添加权限声明
            logCallback("[5/6] 开始添加权限声明...")
            injectManifestPermission(unzippedDir, logCallback)
            logCallback("[5/6] 权限声明添加完成...")

            // 6. 重新打包并签名
            logCallback("[6/6] 开始重新打包与签名...")
            val tempSignedApk = File(workspace, "xyazler_signed.apk")
            repackAndSign(context, baseApkFile, unzippedDir, workspace, tempSignedApk, logCallback)
            logCallback("[6/6] 签名与打包完成")
            logCallback("[*] 新 APK 生成完毕")

            logCallback("[-] 准备触发系统安装...")
            return@withContext tempSignedApk
        } catch (e: Exception) {
            logCallback("[X] 发生异常: ${e.message}")
            logCallback("[X] 堆栈跟踪: ${e.stackTraceToString()}")
            try {
                File(context.cacheDir, "mod_workspace").deleteRecursively()
            } catch (_: Exception) {}
            return@withContext null
        }
    }

    private fun extractApkFromUri(context: Context, uri: Uri, destFile: File, logCallback: (String) -> Unit) {
        val inputStream = context.contentResolver.openInputStream(uri)
            ?: throw IOException("无法从 URI 打开输入流: $uri")
        inputStream.use { input ->
            FileOutputStream(destFile).use { output ->
                val buffer = ByteArray(64 * 1024)
                var bytesRead: Int
                var total = 0L
                val startNano = System.nanoTime()
                var lastLogTime = startNano

                while (input.read(buffer).also { bytesRead = it } != -1) {
                    output.write(buffer, 0, bytesRead)
                    total += bytesRead

                    val now = System.nanoTime()
                    val sinceLastLog = (now - lastLogTime) / 1e9
                    if (sinceLastLog >= 1.0) {
                        val totalMB = total / (1024.0 * 1024.0)
                        val elapsed = (now - startNano) / 1e9
                        logCallback("  [-] 已读取 %.1f MB... (耗时 %.1fs)".format(totalMB, elapsed))
                        lastLogTime = now
                    }
                }
                val totalMB = total / (1024.0 * 1024.0)
                val totalSec = (System.nanoTime() - startNano) / 1e9
                logCallback("  [*] 读取完成，共 %.1f MB (总耗时 %.1fs)".format(totalMB, totalSec))
            }
        }
    }

    private fun unzipApk(apkFile: File, destDir: File, logCallback: (String) -> Unit) {
        destDir.mkdirs()
        var entryCount = 0
        val startNano = System.nanoTime()
        var lastLogTime = startNano
        var lastProgressTime = startNano
        var lastStaleLogTime = 0L
        var lastLoggedCount = 0

        ZipInputStream(FileInputStream(apkFile)).use { zis ->
            var entry = zis.nextEntry
            while (entry != null) {
                val newFile = File(destDir, entry.name)
                if (entry.isDirectory) {
                    newFile.mkdirs()
                } else {
                    newFile.parentFile?.mkdirs()
                    FileOutputStream(newFile).use { fos -> zis.copyTo(fos) }
                }
                zis.closeEntry()
                entryCount++

                val now = System.nanoTime()
                val elapsedTotal = (now - startNano) / 1e9
                val sinceLastLog = (now - lastLogTime) / 1e9

                // 有进展且距上次输出 ≥ 1 秒
                if (entryCount > lastLoggedCount && sinceLastLog >= 1.0) {
                    logCallback("  [-] 已解压 $entryCount 个文件... (耗时 %.1fs)".format(elapsedTotal))
                    lastLogTime = now
                    lastLoggedCount = entryCount
                }
                // 无进展超过 10 秒，且距上次无进展日志 ≥ 10 秒，提示未卡死
                else if (sinceLastLog >= 10.0) {
                    logCallback("  [-] 已解压 $entryCount 个文件... (耗时 %.1fs)".format(elapsedTotal))
                    lastLogTime = now
                }

                entry = zis.nextEntry
            }
        }
        val totalSec = (System.nanoTime() - startNano) / 1e9
        logCallback("  [*] 解压完成，共处理 $entryCount 个文件 (总耗时 %.1fs)".format(totalSec))
    }

    private fun implantSeedLibraries(context: Context, unzippedDir: File, logCallback: (String) -> Unit) {
        val abis = listOf("arm64-v8a", "x86", "x86_64")
        abis.forEach { abi ->
            val libDir = File(unzippedDir, "lib/$abi")
            libDir.mkdirs()
            val targetSo = File(libDir, "libxyazler.so")

            // 从 Assets 目录中精准提取对应架构的种子，绝不混淆！
            try {
                context.assets.open("stub_$abi.so").use { input ->
                    FileOutputStream(targetSo).use { output ->
                        input.copyTo(output)
                    }
                }
            } catch (e: FileNotFoundException) {
                logCallback("  [!]缺失 $abi 版本的 .so 文件")
            }
        }
    }

    private fun injectDalvikBytecode(unzippedDir: File, logCallback: (String) -> Unit) {
        val dexFiles = unzippedDir.listFiles { _, name ->
            name.startsWith("classes") && name.endsWith(".dex")
        }?.sortedBy { it.name } ?: emptyList()

        if (dexFiles.isEmpty()) throw FileNotFoundException("未找到任何 DEX 文件")

        var globalInjected = false
        val targetClassName = "Lcom/bsgamesdk/android/uo/imp/BSGameApplication;"

        for (dexFile in dexFiles) {
            if (globalInjected) break

            logCallback("  [-] 扫描 DEX: ${dexFile.name} ...")
            val originalDex = DexFileFactory.loadDexFile(dexFile, Opcodes.getDefault())

            val foundInCurrentDex = originalDex.classes.any { it.type == targetClassName }

            if (foundInCurrentDex) {
                globalInjected = true
                logCallback("  [-] 在 ${dexFile.name} 中发现目标类: BSGameApplication")

                val rewriterModule = object : RewriterModule() {
                    override fun getClassDefRewriter(rewriters: Rewriters): Rewriter<ClassDef> {
                        return Rewriter { classDef ->
                            val rewrittenClass = super.getClassDefRewriter(rewriters).rewrite(classDef)
                            if (rewrittenClass.type == targetClassName) {
                                val hasClinit = rewrittenClass.methods.any { it.name == "<clinit>" }
                                if (!hasClinit) {
                                    logCallback("  [-] 目标类缺少 <clinit>，正在创建并注入 loadLibrary...")
                                    val loadStringInst = ImmutableInstruction21c(Opcode.CONST_STRING, 0, ImmutableStringReference("xyazler"))
                                    val invokeInst = ImmutableInstruction35c(Opcode.INVOKE_STATIC, 1, 0, 0, 0, 0, 0, ImmutableMethodReference("Ljava/lang/System;", "loadLibrary", listOf("Ljava/lang/String;"), "V"))
                                    val returnVoidInst = ImmutableInstruction10x(Opcode.RETURN_VOID)
                                    val newImpl = ImmutableMethodImplementation(1, listOf(loadStringInst, invokeInst, returnVoidInst), emptyList(), emptyList())
                                    val newClinit = ImmutableMethod(targetClassName, "<clinit>", emptyList(), "V", AccessFlags.STATIC.value or AccessFlags.CONSTRUCTOR.value, emptySet(), emptySet(), newImpl)
                                    val newMethodList = rewrittenClass.methods.toMutableList()
                                    newMethodList.add(newClinit)
                                    return@Rewriter ImmutableClassDef(rewrittenClass.type, rewrittenClass.accessFlags, rewrittenClass.superclass, rewrittenClass.interfaces, rewrittenClass.sourceFile, rewrittenClass.annotations, rewrittenClass.fields, newMethodList)
                                }
                            }
                            return@Rewriter rewrittenClass
                        }
                    }

                    override fun getMethodRewriter(rewriters: Rewriters): Rewriter<Method> {
                        return Rewriter { method ->
                            if (method.definingClass == targetClassName && method.name == "<clinit>") {
                                logCallback("  [-] 修改现有 <clinit>，在方法开头注入 loadLibrary 调用...")
                                val oldImpl = method.implementation
                                if (oldImpl != null) {
                                    val loadStringInst = ImmutableInstruction21c(Opcode.CONST_STRING, 0, ImmutableStringReference("xyazler"))
                                    val invokeInst = ImmutableInstruction35c(Opcode.INVOKE_STATIC, 1, 0, 0, 0, 0, 0, ImmutableMethodReference("Ljava/lang/System;", "loadLibrary", listOf("Ljava/lang/String;"), "V"))
                                    val newInstructions = mutableListOf<Instruction>()
                                    newInstructions.add(loadStringInst)
                                    newInstructions.add(invokeInst)
                                    newInstructions.addAll(oldImpl.instructions.toList())
                                    val newRegisterCount = maxOf(oldImpl.registerCount, 1)
                                    val newImpl = ImmutableMethodImplementation(newRegisterCount, newInstructions, oldImpl.tryBlocks, oldImpl.debugItems)
                                    return@Rewriter ImmutableMethod(method.definingClass, method.name, method.parameters, method.returnType, method.accessFlags, method.annotations, method.hiddenApiRestrictions, newImpl)
                                }
                            }
                            super.getMethodRewriter(rewriters).rewrite(method)
                        }
                    }
                }

                val rewriter = DexRewriter(rewriterModule)
                val rewrittenDex = rewriter.dexFileRewriter.rewrite(originalDex)

                dexFile.delete()
                val dexPool = DexPool(rewrittenDex.opcodes)
                rewrittenDex.classes.forEach { dexPool.internClass(it) }
                dexPool.writeTo(FileDataStore(dexFile))
                logCallback("  [*] ${dexFile.name} 重写完成并已保存")
            }
        }

        if (!globalInjected) {
            throw FileNotFoundException("在所有 DEX 文件中均未找到目标类: $targetClassName")
        }
    }

    private fun injectManifestPermission(unzippedDir: File, logCallback: (String) -> Unit) {
        val manifestFile = File(unzippedDir, "AndroidManifest.xml")
        if (!manifestFile.exists()) {
            throw IllegalStateException("  [!] AndroidManifest.xml 不存在于解压目录中")
        }

        FileInputStream(manifestFile).use { fis ->
            val handler = AXmlHandler(fis)
            val rootNode = handler.document.rootNode

            // 检查是否已存在该权限
            val alreadyHas = rootNode.children.any {
                it.tag == "uses-permission" &&
                it.getAttribute("name")?.value == "android.permission.WRITE_SETTINGS"
            }
            if (!alreadyHas) {
                val permissionNode = AXmlNode("uses-permission", "", rootNode)
                permissionNode.addAttribute("name", AXmlHandler.ANDROID_NAMESPACE, "android.permission.WRITE_SETTINGS")
                rootNode.addChild(permissionNode)

                FileOutputStream(manifestFile).use { fos ->
                    fos.write(handler.toByteArray())
                }
                logCallback("  [-]已添加权限: android.permission.WRITE_SETTINGS")
            } else {
                logCallback("  [-]权限已存在: android.permission.WRITE_SETTINGS")
            }
        }
    }

    private fun repackAndSign(
        context: Context,
        baseApkFile: File,
        unzippedDir: File,
        workspace: File,
        outputApk: File,
        logCallback: (String) -> Unit
    ) {
        val unsignedApk = File(workspace, "unsigned.apk")
        logCallback("  [*] 正在重建 APK 包 (保留原始压缩特征)...")

        ZipFile(baseApkFile).use { originalZip ->
            val totalEntries = originalZip.entries().toList().count { !it.name.startsWith("META-INF/") }
            var processed = 0
            val startNano = System.nanoTime()
            var lastLogTime = startNano
            var lastLoggedCount = 0

            FileOutputStream(unsignedApk).use { fos ->
                ZipOutputStream(fos).use { zos ->
                    val modifiedFiles = unzippedDir.walkTopDown()
                        .filter { it.isFile }
                        .associateBy {
                            it.absolutePath
                                .substring(unzippedDir.absolutePath.length + 1)
                                .replace(File.separatorChar, '/')
                        }

                    for (entry in originalZip.entries()) {
                        if (entry.name.startsWith("META-INF/")) continue
                        processed++

                        val now = System.nanoTime()
                        val elapsedTotal = (now - startNano) / 1e9
                        val sinceLastLog = (now - lastLogTime) / 1e9

                        if (processed > lastLoggedCount && sinceLastLog >= 1.0) {
                            logCallback("  [-] 打包进度: $processed / $totalEntries (耗时 %.1fs)".format(elapsedTotal))
                            lastLogTime = now
                            lastLoggedCount = processed
                        } else if (sinceLastLog >= 10.0) {
                            logCallback("  [-] 打包进度: $processed / $totalEntries (耗时 %.1fs)".format(elapsedTotal))
                            lastLogTime = now
                        }

                        val modifiedFile = modifiedFiles[entry.name]
                        val newEntry = ZipEntry(entry.name)

                        if (modifiedFile != null) {
                            if (entry.method == ZipEntry.STORED) {
                                // 修复 OOM 核心：拒绝一次性读取！改用 File API 获取大小，使用流式分块计算 CRC32
                                newEntry.method = ZipEntry.STORED
                                val fileSize = modifiedFile.length()
                                newEntry.size = fileSize
                                newEntry.compressedSize = fileSize

                                val crc = CRC32()
                                FileInputStream(modifiedFile).use { fis ->
                                    val buffer = ByteArray(64 * 1024) // 64KB 缓冲区
                                    var bytesRead: Int
                                    while (fis.read(buffer).also { bytesRead = it } != -1) {
                                        crc.update(buffer, 0, bytesRead)
                                    }
                                }
                                newEntry.crc = crc.value
                                zos.putNextEntry(newEntry)

                                // 写入时同样使用流式拷贝
                                FileInputStream(modifiedFile).use { fis ->
                                    fis.copyTo(zos)
                                }
                            } else {
                                newEntry.method = ZipEntry.DEFLATED
                                zos.putNextEntry(newEntry)
                                FileInputStream(modifiedFile).use { fis -> fis.copyTo(zos) }
                            }
                        } else {
                            newEntry.method = entry.method
                            if (entry.method == ZipEntry.STORED) {
                                newEntry.size = entry.size
                                newEntry.compressedSize = entry.compressedSize
                                newEntry.crc = entry.crc
                            }
                            zos.putNextEntry(newEntry)
                            originalZip.getInputStream(entry).use { it.copyTo(zos) }
                        }
                        zos.closeEntry()
                    }

                    modifiedFiles.forEach { (relativePath, file) ->
                        if (originalZip.getEntry(relativePath) == null) {
                            val newEntry = ZipEntry(relativePath)
                            newEntry.method = ZipEntry.DEFLATED
                            zos.putNextEntry(newEntry)
                            FileInputStream(file).use { fis -> fis.copyTo(zos) }
                            zos.closeEntry()
                        }
                    }
                }
            }

            val totalSec = (System.nanoTime() - startNano) / 1e9
            logCallback("  [*] APK 打包完成，共处理 $processed 个条目 (总耗时 %.1fs)".format(totalSec))
        }

        logCallback("  [-] 正在加载签名密钥...")
        val ksFile = File(workspace, "temp.keystore")
        context.assets.open("xyazl.keystore").use { input ->
            FileOutputStream(ksFile).use { output -> input.copyTo(output) }
        }

        val key_password = "C++ is the best language in the world, and xylu is the best C++ library."
        val key_name = "xingyuan"
        val (privateKey, certificates) = loadKeyMaterial(ksFile, key_password, key_name)
        val signerConfig = ApkSigner.SignerConfig.Builder(key_name, privateKey, certificates).build()

        logCallback("  [*] 开始 V1/V2 签名...")
        val startNano = System.nanoTime()
        val apkSigner = ApkSigner.Builder(listOf(signerConfig))
            .setInputApk(unsignedApk)
            .setOutputApk(outputApk)
            .setV1SigningEnabled(true)
            .setV2SigningEnabled(true)
            .build()

        apkSigner.sign()
        val totalSec = (System.nanoTime() - startNano) / 1e9
        logCallback("  [*] 签名完成 (总耗时 %.1fs)".format(totalSec))
    }

    // Fix：PKCS12 格式（Android 兼容），单次加载同时取出私钥与证书
    private fun loadKeyMaterial(
        ksFile: File,
        pass: String,
        alias: String
    ): Pair<java.security.PrivateKey, List<java.security.cert.X509Certificate>> {
        Security.addProvider(org.spongycastle.jce.provider.BouncyCastleProvider())
        val ks = KeyStore.getInstance("PKCS12", "SC")
        FileInputStream(ksFile).use { ks.load(it, pass.toCharArray()) }
        val privateKey = ks.getKey(alias, pass.toCharArray()) as java.security.PrivateKey
        val cert = ks.getCertificate(alias) as java.security.cert.X509Certificate
        return Pair(privateKey, listOf(cert))
    }
}