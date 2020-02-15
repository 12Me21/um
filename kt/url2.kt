@file:DependsOn("org.jetbrains.kotlinx:kotlinx-coroutines-core:1.3.3")
@file:DependsOn("com.squareup.okhttp3:okhttp:4.3.1")

import kotlinx.coroutines.*
import okhttp3.*
import java.util.concurrent.TimeUnit
import java.io.IOException

val okHttpClient = OkHttpClient.Builder()
	.readTimeout(0, TimeUnit.DAYS)
	.build()

class Connection() {
	var call: Call? = null
	var room: String? = null
	var running = false
	var start = 0
	
	fun switchRoom(newRoom: String) {
		call?.cancel()
		room = newRoom
		start = 0
		if (!running) {
			run()
		}
	}

	fun stop() {
		running = false
		call?.cancel()
	}
	
	fun run() {
		if (running) {
			return
		}
		GlobalScope.launch {
			running = true
			while (running) {
				var request = Request.Builder().url("http://oboy.smilebasicsource.com/stream/$room?start=$start").build()
				call = okHttpClient.newCall(request)
				try {
					call?.execute()?.body?.string()?.let {
						print(it)
						start += it.length
					}
				} catch(e: IOException) {
				}
			}
		}
	}
}

fun connect(room: String) {
}

fun main() = runBlocking {
	println("starting (enter room name)")
	var conn = Connection()
	while (true) {
		var room = readLine() ?: ""
		if (room == "") {
			break
		}
		println("Switching to room '$room'")
		conn.switchRoom(room)
	}
	conn.stop()
}

//gsquitnw_t
