package com.recipt.homeappliances

sealed class Screen(val route: String) {

    object Home:Screen("Home")
    object Details:Screen("Profile/point"){
        fun endpoint(point:String):String
        {
          return "Profile/$point"
        }

    }

}