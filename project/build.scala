import sbt._

import Keys._
import AndroidKeys._

object General {
  val settings = Defaults.defaultSettings ++ Seq (
    name := "shouty",
    version := "0.4",
    scalaVersion := "2.9.1",
    platformName in Android := "android-8",
    scalacOptions ++= Seq("-deprecation", "-unchecked")
  )

  lazy val fullAndroidSettings =
    General.settings ++
    AndroidProject.androidSettings ++
    TypedResources.settings ++
    AndroidMarketPublish.settings ++ Seq (
      keyalias in Android := "change-me",
      libraryDependencies ++= Seq(
        "org.scalatest" %% "scalatest" % "1.6.1" % "test",
        "net.databinder" %% "unfiltered-netty-server" % "0.5.1"
      )
    )
}

object AndroidBuild extends Build {
  lazy val main = Project (
    "shouty",
    file("."),
    settings = General.fullAndroidSettings ++ 
      AndroidNdk.settings ++
      TypedResources.settings
  )

  lazy val tests = Project (
    "tests",
    file("tests"),
    settings = General.settings ++ AndroidTest.androidSettings
  ) dependsOn main
}
