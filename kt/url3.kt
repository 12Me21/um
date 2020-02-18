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

// a room can be active or inactive
// when the room is active, its data will be kept up to date at all times
// when inactive, the data will not be updated
class Room(name_: String, onData_: (ByteArray)->Unit) {
	val name = name_
	val onData = onData_
	
	var data = ByteArray(0)
	var active = false
	private var call: Call? = null
	
	fun exit() {
		active = false
		call?.cancel()
	}
	
	fun enter() {
		if (active) return
		active = true
		if (data.size > 0) {
			onData(data)
		}
		GlobalScope.launch {
			while (active) {
				requestData()?.let {
					onData(it)
				}
			}
		}.start()
	}
	
	private fun requestData(): ByteArray? {
		// this needs to be safer
		val request = Request.Builder().url("http://oboy.smilebasicsource.com/stream/$name?start=${data.size}").build()
		call = okHttpClient.newCall(request)
		try {
			val response = call?.execute()
			response?.let {
				it.body?.bytes()?.let {
					data += it
					return@requestData it
				}
			}
		} catch(e: IOException) {
			// cancelling a call throws an exception
			// for some reason...
			if (call?.isCanceled() == false) {
				throw e
			}
			return null
		}
		return null
	}
}

fun main() = runBlocking {
	println("starting (enter room name)")
	var room: Room? = null
	while (true) {
		var name = readLine() ?: ""
		room?.exit()
		if (name == "") {
			break
		}
		println("Switching to room '$name'")
		room = Room(name, { x -> print(x.toString(Charsets.UTF_8)); System.out.flush()})
		room.enter()
	}
}

//gsquitnw_tn
