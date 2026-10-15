def calculate_safety_score(ride: dict) -> tuple[int, str]:
    """Transparent prototype score. Replace with an ML model after real data exists."""
    score = 100
    reasons = []

    max_speed = float(ride.get("max_speed") or 0)
    harsh_accel = int(ride.get("harsh_accel_count") or 0)
    harsh_brake = int(ride.get("harsh_brake_count") or 0)
    helmet_off = int(ride.get("helmet_off_events") or 0)
    alcohol = int(ride.get("alcohol_events") or 0)
    accidents = int(ride.get("accident_events") or 0)

    if max_speed > 80:
        score -= min(20, int((max_speed - 80) / 3) + 5)
        reasons.append("reduce excessive speed")
    elif max_speed > 60:
        score -= 5
        reasons.append("maintain a slightly lower speed")

    if harsh_accel:
        score -= min(15, harsh_accel * 3)
        reasons.append("avoid sudden acceleration")
    if harsh_brake:
        score -= min(15, harsh_brake * 3)
        reasons.append("brake more gradually")
    if helmet_off:
        score -= min(30, helmet_off * 15)
        reasons.append("keep the helmet worn throughout the ride")
    if alcohol:
        score -= 50
        reasons.append("never ride after alcohol is detected")
    if accidents:
        score -= min(35, accidents * 25)
        reasons.append("review the detected accident event")

    score = max(0, min(100, score))
    if score >= 90:
        summary = "Excellent riding behaviour."
    elif score >= 75:
        summary = "Good riding behaviour."
    elif score >= 50:
        summary = "Riding behaviour needs improvement."
    else:
        summary = "High-risk riding behaviour detected."

    advice = " Keep following safe riding practices." if not reasons else " Please " + ", and ".join(reasons) + "."
    return score, summary + advice

