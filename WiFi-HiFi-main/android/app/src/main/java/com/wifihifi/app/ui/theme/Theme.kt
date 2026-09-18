package com.wifihifi.app.ui.theme

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable

private val DarkColorScheme = darkColorScheme(
    primary = HiFiGold,
    secondary = HiFiText,
    background = HiFiBackground,
    surface = HiFiSurface,
    onPrimary = HiFiBackground,
    onSecondary = HiFiBackground,
    onBackground = HiFiText,
    onSurface = HiFiText
)

@Composable
fun WiFiHiFiTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = DarkColorScheme,
        typography = Typography,
        content = content
    )
}
