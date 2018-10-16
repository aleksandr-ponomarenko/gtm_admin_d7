<?php

// Factory for GTM service
function _gtm_admin_get_google_gtm_service() {
  $service =& drupal_static(__FUNCTION__);

  if (empty($service)) {
    $client = _gtm_admin_get_google_client();
    if (!$client) {
      return $client;
    }
    $service = new Google_Service_TagManager($client);
  }

  return $service;
}

// Get Google Client helper (with auth)
function _gtm_admin_get_google_client() {
  $file_error = FALSE;
  $file_json = variable_get('gtm_admin_settings_credentials_file', '');
  if (empty($file_json)) {
    $file_error = TRUE;
  }

  $credentials_file = file_load($file_json);
  if (!isset($credentials_file->uri) || empty($credentials_file->uri)) {
    $file_error = TRUE;
  }

  $credentials_file_path = drupal_realpath($credentials_file->uri);

  if (!file_exists($credentials_file_path)) {
    $file_error = TRUE;
  }

  if ($file_error) {
    watchdog('gtm-admin', 'Credential file not found, please visit settings page and set right one up.', [], WATCHDOG_ERROR);
    drupal_set_message(t('Credential file not found, please visit !settings_page and set right one up.', [
      '!settings_page' => l('settings page', 'admin/config/search/gtm-admin-config'),
    ]), 'warning');
    return FALSE;
  }

  if (!$credentials_file->status) {
    $credentials_file->status = FILE_STATUS_PERMANENT;
    file_save($credentials_file);
  }

  if (!class_exists('Google_Client')) {
    watchdog('gtm-admin', 'Google api library not installed.', [], WATCHDOG_ERROR);
    drupal_set_message(t('Google api library not installed. Please read documentation and install it using composer.'), 'error');
    return FALSE;
  }

  $client = new Google_Client();
  $client->setScopes([
    Google_Service_TagManager::TAGMANAGER_READONLY,
    Google_Service_TagManager::TAGMANAGER_EDIT_CONTAINERS,
    Google_Service_TagManager::TAGMANAGER_EDIT_CONTAINERVERSIONS,
    Google_Service_TagManager::TAGMANAGER_PUBLISH,
  ]);

  switch (variable_get('gtm_admin_settings_credentials_type', 'service')) {
    // Service account auth.
    case 'service':
      putenv('GOOGLE_APPLICATION_CREDENTIALS=' . $credentials_file_path);
      $client->useApplicationDefaultCredentials();
      break;

    // User account auth.
    case 'user':
      $client->setAuthConfig($credentials_file_path);
      $redirect_url = 'http://drupal7.org/admin/gtm-admin-page';
      $client->setRedirectUri($redirect_url);
      if (isset($_GET['code'])) {
        $token = $client->fetchAccessTokenWithAuthCode($_GET['code']);
        $client->setAccessToken($token);
        $_SESSION['gtm-token'] = $token;
        drupal_goto($redirect_url);
      }
      if (isset($_SESSION['gtm-token']) && !empty($_SESSION['gtm-token'])) {
        $client->setAccessToken($_SESSION['gtm-token']);
        $client->isAccessTokenExpired();
        if ($client->isAccessTokenExpired() === TRUE) {
          unset($_SESSION['gtm-token']);
          drupal_goto($client->createAuthUrl());
        }
      }
      else {
        drupal_goto($client->createAuthUrl());
      }
      break;

    default:
      watchdog('gtm-admin', 'Something goes wrong with Google authorisation, check settings.', [], WATCHDOG_ERROR, url(GTM_ADMIN_LISTING_PAGE_URL));
      drupal_set_message(t('Something goes wrong with Google authorisation, check !settings_page.', [
        '!settings_page' => l('settings page', 'admin/config/search/gtm-admin-config'),
      ]), 'error');
      return FALSE;
  }

  return $client;
}

// Helper formatting fucntion.
function _gtm_admin_get_parent_string($params = []) {
  $parent = '';
  foreach ($params as $key => $param) {
    if (!empty($parent)) {
      $parent .= '/';
    }

    if (!is_numeric($key)) {
      $parent .= $key . '/' . $param;
    }
    else {
      $parent .= $param;
    }
  }
  return $parent;
}

// Helper formatting fucntion.
function _gtm_admin_get_param_string($params = []) {
  $param_string = '';
  foreach ($params as $param) {
    if (!empty($param_string)) {
      $param_string .= '/';
    }
    $param_string .= $param;
  }

  return $param_string;
}
