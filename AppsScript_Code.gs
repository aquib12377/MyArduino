/*
  =========================================================
  Google Apps Script backend for ESP32 Attendance System
  =========================================================
  Bind this script to your Google Sheet (Extensions > Apps Script).

  Expected sheets/columns:

  "Users" sheet:
     A: Name            (you type this manually to register someone)
     B: FingerprintID    (left BLANK until the ESP32 enrolls them)
     C: Status           ("Pending" / "Enrolled" - set automatically)
     D: DateEnrolled      (set automatically)

  "Attendance" sheet:
     A: Timestamp
     B: FingerprintID
     C: Name
     D: Time (HH:mm:ss, for easy reading)

  Deployment:
     1. Extensions > Apps Script, paste this code.
     2. Deploy > New deployment > Web app.
        - Execute as: Me
        - Who has access: Anyone
     3. Copy the generated /exec URL into GAS_URL in the ESP32 code.
     4. Re-deploy (new version) any time you edit this script,
        otherwise the ESP32 keeps hitting the old code.
  =========================================================
*/

const USERS_SHEET = "Users";
const ATTENDANCE_SHEET = "Attendance";

function doGet(e) {
  const action = e.parameter.action;
  const ss = SpreadsheetApp.getActiveSpreadsheet();
  let result = {};

  try {
    if (action === "checkPending") {
      result = checkPendingUser(ss);
    } else if (action === "updateEnrollment") {
      result = updateEnrollment(ss, e.parameter.row, e.parameter.fpid, e.parameter.name);
    } else if (action === "logAttendance") {
      result = logAttendance(ss, e.parameter.fpid);
    } else {
      result = { status: "error", message: "Unknown action: " + action };
    }
  } catch (err) {
    result = { status: "error", message: err.toString() };
  }

  return ContentService.createTextOutput(JSON.stringify(result))
    .setMimeType(ContentService.MimeType.JSON);
}

// Finds the first row in Users that has a Name but no FingerprintID yet.
function checkPendingUser(ss) {
  const sheet = ss.getSheetByName(USERS_SHEET);
  if (!sheet) return { status: "error", message: "Users sheet not found" };

  const data = sheet.getDataRange().getValues();
  // Row 0 is the header, so start from row index 1
  for (let i = 1; i < data.length; i++) {
    const name = data[i][0];
    const fpid = data[i][1];
    if (name && (fpid === "" || fpid === null || fpid === undefined)) {
      return { status: "pending", row: i + 1, name: name };
    }
  }
  return { status: "none" };
}

// Writes the new FingerprintID back into the Users sheet after enrollment.
function updateEnrollment(ss, row, fpid, name) {
  const sheet = ss.getSheetByName(USERS_SHEET);
  if (!sheet) return { status: "error", message: "Users sheet not found" };

  row = parseInt(row, 10);
  if (!row || row < 2) return { status: "error", message: "Invalid row" };

  sheet.getRange(row, 2).setValue(fpid);          // Column B: FingerprintID
  sheet.getRange(row, 3).setValue("Enrolled");    // Column C: Status
  sheet.getRange(row, 4).setValue(new Date());    // Column D: DateEnrolled

  return { status: "success", message: "Enrollment saved for " + name };
}

// Looks up the name for a matched FingerprintID and appends an attendance row.
function logAttendance(ss, fpid) {
  const usersSheet = ss.getSheetByName(USERS_SHEET);
  const attSheet = ss.getSheetByName(ATTENDANCE_SHEET);
  if (!usersSheet || !attSheet) {
    return { status: "error", message: "Required sheet(s) not found" };
  }

  const data = usersSheet.getDataRange().getValues();
  let name = null;
  for (let i = 1; i < data.length; i++) {
    if (String(data[i][1]) === String(fpid) && String(fpid).length > 0) {
      name = data[i][0];
      break;
    }
  }

  if (!name) {
    return { status: "error", message: "No user found for FingerprintID " + fpid };
  }

  const now = new Date();
  const timeStr = Utilities.formatDate(now, Session.getScriptTimeZone(), "HH:mm:ss");
  attSheet.appendRow([now, fpid, name, timeStr]);

  return { status: "success", name: name };
}
