@file:DependsOn("org.jetbrains.kotlinx:kotlinx-coroutines-core:1.3.3")
@file:DependsOn("com.squareup.okhttp3:okhttp:4.3.1")
@file:DependsOn("ru.gildor.coroutines:kotlin-coroutines-okhttp:1.0")

import kotlinx.coroutines.*
import okhttp3.*
import java.util.concurrent.TimeUnit
import java.io.IOException
import ru.gildor.coroutines.okhttp.*

val okHttpClient = OkHttpClient.Builder()
	.readTimeout(0, TimeUnit.DAYS)
	.build()

fun buildRequest(url: String): Request {
	return Request.Builder().url(url).build()
}

class Connection(callback_: (String)->Unit) {
	val callback = callback_
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
		running = true
		GlobalScope.launch {
			while (running) {
				val request = buildRequest("http://oboy.smilebasicsource.com/stream/$room?start=$start")
				call = okHttpClient.newCall(request)
				try {
					val response = call?.execute()
					response?.let {
						it.body?.string()?.let {
							callback(it)
							start += it.length
						}
					}
				} catch(e: IOException) {
				}
			}
		}.start()
	}
}

fun main() = runBlocking {
	println("starting (enter room name)")
	var conn = Connection({ x -> print(x); System.out.flush()})
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

//gsquitnw_tn
