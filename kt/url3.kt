@file:DependsOn("org.jetbrains.kotlinx:kotlinx-coroutines-core:1.3.3")
@file:DependsOn("com.squareup.okhttp3:okhttp:4.3.1")

import kotlinx.coroutines.*
import okhttp3.*
import java.util.concurrent.TimeUnit
import java.io.IOException

val okHttpClient = OkHttpClient.Builder()
	.readTimeout(0, TimeUnit.DAYS)
	.build()

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
		if (data.size > 0) { // if there is already cached data
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
	val rooms: HashMap<String, Room> = hashMapOf()
	var current: Room? = null
	while (true) {
		val name = readLine() ?: ""
		current?.exit()
		if (name == "") {
			break
		}
		
		current = rooms.get(name)
		if (current == null) {
			current = Room(name, { x -> print(x.toString(Charsets.UTF_8)); System.out.flush()})
			rooms.put(name, current)
		}
		println("Switching to room '$name'")
		current?.enter()
	}
}

//gsquitnw_tn
