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
		GlobalScope.launch {
			running = true
			while (true) {
				var request = Request.Builder().url("http://oboy.smilebasicsource.com/stream/$room?start=$start").build()
				var call_ = okHttpClient.newCall(request)
				call = call_
				try {
					var x = call_.execute().body?.string()
					x?.let {
						print(x)
						start += x.length
					}
				} catch(e: IOException) {
				}
				if (!running) {
					break;
				}
			}
		}
	}
}

fun connect(room: String) {
}

fun main() = runBlocking {
	println("OK! ♥")
	var conn = Connection()
	conn
	conn.run()
	while (true) {
		var room = readLine()
		room?.let {
			conn.switchRoom(room)
		}
	}
	println("hecko")
}

//gsquitnw_t
