package com.iot.distancemonitor

import android.animation.ValueAnimator
import android.content.Context
import android.graphics.*
import android.util.AttributeSet
import android.view.View
import android.view.animation.LinearInterpolator
import kotlin.math.sin

class WaterTankView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    // Water level 0..100
    var waterLevelPercent: Float = 0f
        set(value) {
            field = value.coerceIn(0f, 100f)
            invalidate()
        }

    // Colours
    private val waterColor = Color.parseColor("#FF26A8B5")
    private val waterDarkColor = Color.parseColor("#FF1E8A95")
    private val tankOutlineColor = Color.parseColor("#FFCCCCCC")
    private val tankBodyColor = Color.parseColor("#FF2A2A3E")
    private val bandColor = Color.parseColor("#FF1E8A95")
    private val textColor = Color.WHITE
    private val lidColor = Color.parseColor("#FF888888")

    // Paints
    private val tankOutlinePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = tankOutlineColor
        style = Paint.Style.STROKE
        strokeWidth = 4f
    }
    private val tankFillPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = tankBodyColor
        style = Paint.Style.FILL
    }
    private val waterPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = waterColor
        style = Paint.Style.FILL
    }
    private val waterDarkPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = waterDarkColor
        style = Paint.Style.FILL
    }
    private val bandPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = bandColor
        style = Paint.Style.STROKE
        strokeWidth = 3f
    }
    private val percentPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = textColor
        textAlign = Paint.Align.CENTER
        isFakeBoldText = true
    }
    private val labelPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = textColor
        textAlign = Paint.Align.CENTER
    }
    private val lidPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = lidColor
        style = Paint.Style.FILL
    }
    private val lidOutlinePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = tankOutlineColor
        style = Paint.Style.STROKE
        strokeWidth = 3f
    }

    // Wave animation
    private var waveOffset = 0f
    private val waveAnimator = ValueAnimator.ofFloat(0f, 360f).apply {
        duration = 3000
        repeatCount = ValueAnimator.INFINITE
        interpolator = LinearInterpolator()
        addUpdateListener {
            waveOffset = it.animatedValue as Float
            invalidate()
        }
    }

    private val wavePath = Path()
    private val tankPath = Path()
    private val clipPath = Path()

    init {
        waveAnimator.start()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)

        val w = width.toFloat()
        val h = height.toFloat()
        val density = resources.displayMetrics.density

        // Tank dimensions
        val tankMargin = w * 0.1f
        val tankLeft = tankMargin
        val tankRight = w - tankMargin
        val tankWidth = tankRight - tankLeft

        val lidHeight = h * 0.06f
        val topDomeHeight = h * 0.1f
        val bottomDomeHeight = h * 0.06f
        val tankTop = lidHeight + topDomeHeight * 0.3f
        val tankBottom = h - bottomDomeHeight

        // --- Draw lid (cap on top) ---
        val lidWidth = tankWidth * 0.25f
        val lidLeft = w / 2 - lidWidth / 2
        val lidRight = w / 2 + lidWidth / 2
        val lidTop = 0f
        val lidBottom = lidHeight + 4f
        val lidRect = RectF(lidLeft, lidTop, lidRight, lidBottom)
        canvas.drawRoundRect(lidRect, 6 * density, 6 * density, lidPaint)
        canvas.drawRoundRect(lidRect, 6 * density, 6 * density, lidOutlinePaint)

        // --- Build tank body path (rounded cylinder shape) ---
        val cornerRadius = tankWidth * 0.12f

        tankPath.reset()
        // Top dome (elliptical arc)
        val topOvalRect = RectF(tankLeft, tankTop, tankRight, tankTop + topDomeHeight * 2)
        tankPath.moveTo(tankLeft, tankTop + topDomeHeight)
        tankPath.arcTo(topOvalRect, 180f, 180f, false)
        // Right side
        tankPath.lineTo(tankRight, tankBottom - bottomDomeHeight)
        // Bottom dome
        val bottomOvalRect = RectF(tankLeft, tankBottom - bottomDomeHeight * 2, tankRight, tankBottom)
        tankPath.arcTo(bottomOvalRect, 0f, 180f, false)
        // Left side
        tankPath.lineTo(tankLeft, tankTop + topDomeHeight)
        tankPath.close()

        // Draw tank body fill
        canvas.drawPath(tankPath, tankFillPaint)

        // --- Water fill ---
        val waterAreaTop = tankTop + topDomeHeight * 0.5f
        val waterAreaBottom = tankBottom - bottomDomeHeight * 0.5f
        val waterAreaHeight = waterAreaBottom - waterAreaTop
        val waterTop = waterAreaBottom - (waterAreaHeight * waterLevelPercent / 100f)

        if (waterLevelPercent > 0) {
            canvas.save()
            canvas.clipPath(tankPath)

            // Draw water body
            canvas.drawRect(tankLeft, waterTop + 8f, tankRight, tankBottom, waterPaint)

            // Draw wave at top of water
            val waveAmplitude = 6f * density
            val waveLength = tankWidth / 1.5f
            wavePath.reset()
            wavePath.moveTo(tankLeft - 10f, waterTop)
            var x = tankLeft - 10f
            while (x <= tankRight + 10f) {
                val y = waterTop + sin(((x / waveLength * 360f + waveOffset) * Math.PI / 180f)).toFloat() * waveAmplitude
                wavePath.lineTo(x, y)
                x += 2f
            }
            wavePath.lineTo(tankRight + 10f, tankBottom + 10f)
            wavePath.lineTo(tankLeft - 10f, tankBottom + 10f)
            wavePath.close()
            canvas.drawPath(wavePath, waterPaint)

            // Second wave layer (slightly darker, offset)
            wavePath.reset()
            wavePath.moveTo(tankLeft - 10f, waterTop + 3f)
            x = tankLeft - 10f
            while (x <= tankRight + 10f) {
                val y = waterTop + 3f + sin(((x / waveLength * 360f + waveOffset + 120f) * Math.PI / 180f)).toFloat() * waveAmplitude * 0.6f
                wavePath.lineTo(x, y)
                x += 2f
            }
            wavePath.lineTo(tankRight + 10f, tankBottom + 10f)
            wavePath.lineTo(tankLeft - 10f, tankBottom + 10f)
            wavePath.close()
            canvas.drawPath(wavePath, waterDarkPaint)

            canvas.restore()
        }

        // --- Draw horizontal bands (ribs on the tank) ---
        val bandCount = 5
        val bandSpacing = (tankBottom - tankTop - topDomeHeight) / (bandCount + 1)
        for (i in 1..bandCount) {
            val bandY = tankTop + topDomeHeight + bandSpacing * i
            canvas.drawLine(tankLeft + cornerRadius * 0.3f, bandY, tankRight - cornerRadius * 0.3f, bandY, bandPaint)
        }

        // --- Draw tank outline ---
        tankOutlinePaint.strokeWidth = 3f * density
        canvas.drawPath(tankPath, tankOutlinePaint)

        // --- Percentage text centered on tank ---
        percentPaint.textSize = tankWidth * 0.18f
        val percentText = String.format("%.1f%%", waterLevelPercent)
        val labelText = "Water Tank Level"
        labelPaint.textSize = tankWidth * 0.09f

        val textCenterY = tankTop + topDomeHeight + (waterAreaHeight * 0.15f)
        canvas.drawText(labelText, w / 2, textCenterY, labelPaint)
        canvas.drawText(percentText, w / 2, textCenterY + percentPaint.textSize * 1.1f, percentPaint)
    }

    override fun onDetachedFromWindow() {
        waveAnimator.cancel()
        super.onDetachedFromWindow()
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        if (!waveAnimator.isRunning) waveAnimator.start()
    }
}
