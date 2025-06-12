<?php

require("config.php");

// Check if binary data was uploaded
if (!isset($_FILES["binary_data"])) {
    http_response_code(400);
    echo json_encode(["error" => "No binary data received"]);
    exit;
}

try {
    $mysqli = mysqli_connect("localhost", $username, $pass, $db);
    
    if (!$mysqli) {
        throw new Exception("Database connection failed: " . mysqli_connect_error());
    }
    
    // Read the binary data
    $binaryData = file_get_contents($_FILES["binary_data"]["tmp_name"]);
    
    if ($binaryData === false) {
        throw new Exception("Failed to read uploaded binary data");
    }
    
    // Insert into database
    $stmt = $mysqli->prepare("INSERT INTO images (orig_name, epd_bin) VALUES (?, ?)");
    if (!$stmt) {
        throw new Exception("Prepare failed: " . $mysqli->error);
    }
    
    $origName = "js_converted_" . date('Y-m-d_H-i-s') . ".bin";
    $stmt->bind_param("sb", $origName, $null);
    $stmt->send_long_data(1, $binaryData);
    
    if (!$stmt->execute()) {
        throw new Exception("Execute failed: " . $stmt->error);
    }
    
    $imageId = $mysqli->insert_id;
    
    $stmt->close();
    $mysqli->close();
    
    // Clean up temporary file
    unlink($_FILES["binary_data"]["tmp_name"]);
    
    echo json_encode([
        "success" => true, 
        "id" => $imageId,
        "message" => "Binary data stored successfully"
    ]);
    
} catch (Exception $e) {
    http_response_code(500);
    echo json_encode(["error" => $e->getMessage()]);
}

?> 