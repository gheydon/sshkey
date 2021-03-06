<?php

function sshkey_page_list_public_keys($type, $id = NULL) {
  drupal_add_css(drupal_get_path('module', 'sshkey') . '/css/sshkey.css');

  $keys = sshkey_user_public_keys_list($type, $id);

  $header = array(
    array('data' => t('Fingerprint'), 'class' => 'sshkey-public-key-fingerprint'),
    array('data' => t('Name'),        'class' => 'sshkey-public-key-name'),
    array('data' => t('Created'),     'class' => 'sshkey-public-key-created'),
    array('data' => t('Operations'),  'class' => 'sshkey-public-key-operations'),
  );
  $rows = array();

  foreach ($keys as $key) {
    $segmented = '';
    for ($i=0; $i<8; $i+=2) {
      $segmented .= substr($key['fingerprint'], $i, 2) . ':';
    }

    $data = array(
      'fingerprint' => array(
        'data'  => $segmented . '...',
        'class' => 'sshkey-public-key-key',
      ),
      'name' => array(
        'data'  => $key['name'],
        'class' => 'sshkey-public-key-name',
      ),
      'created' => array(
        'data'  => format_date($key['created']),
        'class' => 'sshkey-public-key-created',
      ),
    );

    $operations = module_invoke_all('link', 'sshkey', $key);
    drupal_alter('link', $operations, $key);
    
    $rows[] = array(
      'data' => $data + array(
        'operations' => array(
          'data'  => theme('links', $operations),
          'class' => 'sshkey-public-key-operations',
        ),
      ),
      'class' => 'sshkey-public-key-operations',
    );
  }

  $table = theme('sshkey_table', $header, $rows, array('id' => 'sshkey-public-list-key'));
  $form = drupal_get_form('sshkey_add_public_key_form', $type, $id);
  return theme('sshkey_public_key_list_page', $table, $form);
}

function sshkey_add_public_key_form($form_state, $type, $id) {
  $form = array();

  $form['obj_type'] = array(
    '#type' => 'value',
    '#value' => $type,
  );
  $form['obj_id'] = array(
    '#type' => 'value',
    '#value' => $id,
  );

  $form['name'] = array(
    '#type' => 'textfield',
    '#title' => t('Name'),
    '#description' => t('The name of the public key. Will be filled in from the key comment if omitted.'),
    '#size' => 40,
    '#maxlength' => 255,
  );
  
  $form['public_key'] = array(
    '#type' => 'textarea',
    '#title' => t('Public key'),
    '#description' => t('The contents of your public key file.'),
    '#cols' => 40,
    '#rows' => 5,
    '#required' => TRUE,
  );

  $form['save'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
  );

  return $form;
}

function sshkey_add_public_key_form_validate($form, $form_state) {
  module_load_include('inc', 'sshkey');
  $values = $form_state['values'];
  $type = $values['obj_type'];
  $id = $values['obj_id'];

  try {
    // Parse the key
    $pubkey = sshkey_public_key_parse($values['public_key']);

    // Check if the key is blacklisted.
    if (sshkey_public_key_blacklisted($pubkey['pubkey'])) {
      form_set_error('public_key', t('The key you have used is blacklisted'));
    }

    // Check if the key exists
    $exists = sshkey_public_key_load($pubkey['fingerprint']);
    if ($exists) {
      // Be a bit helpful and tell the user if the key has been added by herself,
      // or if it's been added by someone else. Not sure how to treat the double-
      // add scenario.
      if ($exists['obj_type'] == $type && $exists['obj_id'] == $id) {
        form_set_error('public_key', t('You have already added this public key.'));
      }
      else {
        form_set_error('public_key', t('This public key has already been added by someone else, contact the webmaster at @email if you have any questions or suspect malicious intent.', array(
          '@email' => variable_get('site_mail', ''),
        )));
      }
    }
  }
  catch (Exception $e) {
    form_set_error('public_key', t('There was a problem with the key: !message', array(
      '!message' => $e->getMessage(),
    )));
  }
}

function sshkey_add_public_key_form_submit($form, $form_state) {
  $values = $form_state['values'];
  $type = $values['obj_type'];
  $id = $values['obj_id'];

  try {
    sshkey_public_key_save($type, $id, $values['public_key'], $values['name']);
  }
  catch (Exception $e) {
    drupal_set_message(t('There was a problem saving your key: !message', array(
      '!message' => $e->getMessage(),
    )), 'error');
  }
}

function sshkey_edit_public_key_form($form_state, $type, $key) {
  $form = array();
  $data = sshkey_get_public_key_data($key['fingerprint']);

  $segmented = array();
  for ($i=0; $i<strlen($key['fingerprint']); $i+=2) {
    $segmented[] = substr($key['fingerprint'], $i, 2);
  }
  $readable_fingerprint = join($segmented, ':');

  $form['key_object'] = array(
    '#type' => 'value',
    '#value' => $data,
  );

  $form['name'] = array(
    '#type' => 'textfield',
    '#title' => t('Name'),
    '#description' => t('The name that should be used for this key.'),
    '#size' => 40,
    '#maxlength' => 255,
    '#default_value' => $key['name'],
  );

  $form['created'] = array(
    '#type' => 'item',
    '#title' => t('Created'),
    '#value' => format_date($key['created']),
  );

  $form['fingerprint'] = array(
    '#type' => 'item',
    '#title' => t('Fingerprint'),
    '#value' => $readable_fingerprint,
  );

  $form['public_key'] = array(
    '#type' => 'textarea',
    '#title' => t('Public key'),
    '#disabled' => TRUE,
    '#value' => $data['pubkey'],
    '#rows' => 8,
  );

  $form['save'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
  );

  return $form;
}

function sshkey_edit_public_key_form_submit($form, $form_state) {
  $values = $form_state['values'];

  $key = $values['key_object'];
  $name = empty($values['name']) ? $key['comment'] : $values['name'];

  sshkey_public_key_update($key['fingerprint'], $name);
  drupal_goto();
}

function sshkey_delete_public_key_form($form_state, $type, $key) {
  $form['key_object'] = array(
    '#type' => 'value',
    '#value' => $key
  );

  return confirm_form($form,
    t('Are you sure you want to delete this public ssh key?'),
    'user/' . $key['uid'] . '/sshkeys',
    t('Your\'e about to delete a public key, effectively disabling access for any services that use it. Are you sure that you want to do this?'),
    t('Delete')
    );
}

function sshkey_delete_public_key_form_submit($form, $form_state) {
  $key = $form_state['values']['key_object'];
  sshkey_public_key_delete($key['fingerprint']);
  drupal_goto('user/' . $key['uid'] . '/sshkeys');
}