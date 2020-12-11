// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const challengeURL =
    'https://chromeos.google.com/partner/console/cr50reset?challenge=';

const state = {};
const setStateAndRender = (newState) => {
  Object.assign(state, newState);
  try {
    render();
  } catch(error) {
    console.error(error);
    alert(error);
  }
};

const fetchAPI = (path, data={}) => {
  return fetch(path, {method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(data)});
};

const getBooleanText = (value, trueText, trueColor, falseText, falseColor) => {
  const ele = document.createElement('span');
  if (value === undefined) {
    ele.innerText = 'undefined';
    return;
  }
  ele.innerText = value ? trueText : falseText;
  ele.style.color = value ? trueColor : falseColor;
  return ele;
};
const renderTable = (ele, data) => {
  if (!data)
    return;
  const table = ele.appendChild(document.createElement('table'));
  table.style = 'border: 1px solid black; margin: 5px';
  for (let key in data) {
    const style = 'border: 1px solid black; padding: 5px';
    const tr = table.appendChild(document.createElement('tr'));
    const tdKey = tr.appendChild(document.createElement('td'));
    tdKey.style = style;
    tdKey.innerText = key;
    const tdValue = tr.appendChild(document.createElement('td'));
    tdValue.style = style;
    console.log(data[key])
    if (!data[key]);
    else if (typeof data[key] === 'string') {
      tdValue.innerText = data[key];
    } else {
      tdValue.appendChild(data[key]);
    }
  }
};
const renderButton = (ele, text, onclick) => {
  const btn = ele.appendChild(document.createElement('button'));
  if (state.isLoading) {
    text += ' (Loading...)';
  }
  btn.innerText = text;
  btn.onclick = onclick;
  btn.disabled = state.isLoading;
  btn.style = 'margin: 5px; padding: 5px';
};
const renderLink = (ele, text, href, target) => {
  const a = ele.appendChild(document.createElement('a'));
  a.href = href;
  a.innerText = text;
  a.target = target;
  a.style = 'display: block; margin: 5px; padding: 5px;';
};
const renderText = (ele, text, type='span') => {
  const span = ele.appendChild(document.createElement(type));
  span.innerText = text;
  if (type[0] == 'h') {
    const hr = root.appendChild(document.createElement('hr'));
    hr.style = 'margin: -10px 0 20px 0;';
  }
}
const renderInput = (ele, id, label) => {
  const div = ele.appendChild(document.createElement('div'));
  div.style = 'margin: 5px; padding: 5px;';
  const b = div.appendChild(document.createElement('b'));
  b.innerText = label + ': ';
  const input = div.appendChild(document.createElement('input'));
  input.id = id;
  const state_key = 'input_' + id;
  input.type = 'text';
  input.value = state[state_key] || '';
  input.onchange = e => {
    state[state_key] = e.target.value;
  };
};
const renderSelect = (ele, id, label, keys, values) => {
  const div = ele.appendChild(document.createElement('div'));
  div.style = 'margin: 5px; padding: 5px;';
  const b = div.appendChild(document.createElement('b'));
  b.innerText = label + ': ';
  const select = div.appendChild(document.createElement('select'));
  for (let i = 0; i < keys.length; i++) {
    const option = select.appendChild(document.createElement('option'));
    option.innerText = keys[i];
    option.value = values[i];
  }
  select.id = id;
  const state_key = 'input_' + id;
  select.value = state[state_key] || '';
  select.onchange = e => {
    state[state_key] = e.target.value;
  };
};
const handleScan = async (event) => {
  setStateAndRender({isLoading: true, message: 'Scanning...'});
  const resp = await fetchAPI('/scan');
  const scanData = await resp.json();
  if (scanData.isRestricted && event != undefined) {
    // When user press scan button, automatically open challenge page.
    window.open(challengeURL + scanData.challenge, 'challenge');
  }
  let message = undefined;
  if (scanData.isRestricted && !scanData.challenge) {
    message = 'Cannot generate rma challenge!!! Try again.'
  }
  setStateAndRender({isLoading: false, scanData, extractData: undefined, message, input_authcode: undefined, input_board: undefined});
};
const renderScan = (ele) => {
  renderText(root, 'Scan the device', 'h4');
  renderButton(ele, 'Scan', handleScan);
  if (!state.scanData)
    return;
  if (state.scanData.error) {
    renderTable(ele, state.scanData);
    return;
  }
  const { cr50SerialName, rlz, referenceBoard, isRestricted, isTestlabEnabled, supportedBoards } = state.scanData;
  renderTable(ele, {
    'Cr50 Serial Name': cr50SerialName,
    'RLZ Code': rlz,
    'Reference Board': getBooleanText(supportedBoards.indexOf(referenceBoard) != -1, `${referenceBoard} (Supported)`, 'green', `${referenceBoard} (Not Supported)`, 'red'),
    'Open Or Lock': getBooleanText(isRestricted, 'Lock', 'red', 'Open', 'green'),
    'Testlab Enabled': getBooleanText(isTestlabEnabled, 'Enabled', 'green', 'Disabled', 'red'),
  });
};
const handleUnlock = async () => {
  setStateAndRender({isLoading: true, message: 'Unlocking...'});
  const authcode = state.input_authcode;
  const resp = await fetchAPI('/unlock', {cr50SerialName: state.scanData.cr50SerialName, authcode});
  const data = await resp.json();
  if (data.success) {
    /* Re-scan the device */
    await handleScan();
  } else {
    setStateAndRender({isLoading: false, message: 'Unlock failed.'});
  }
};
const renderUnlock = (ele) => {
  renderText(root, 'Unlock the device', 'h4');
  renderLink(ele, 'Get Auth Code', challengeURL + state.scanData.challenge, 'challenge');
  renderInput(ele, 'authcode', 'Authcode');
  renderButton(ele, 'Unlock', handleUnlock);
  renderTable(ele, state.unlockResult);
};
const handleExtract = async () => {
  setStateAndRender({isLoading: true, message: 'Extracting...'});
  let board = state.input_board;
  if (!board) {
    board = state.scanData.referenceBoard;
  }
  if (state.scanData.supportedBoards.indexOf(board) == -1) {
    setStateAndRender({isLoading: false, message: `Board "${board}" is not supported by HWID Extractor.`});
    return;
  }
  const resp = await fetchAPI('/extract', {cr50SerialName: state.scanData.cr50SerialName, board});
  setStateAndRender({isLoading: false, extractData: await resp.json(), message: undefined});
};
const renderExtract = (ele) => {
  renderText(ele, 'Extract HWID and Serial No.', 'h4');
  const keys = [`${state.scanData.referenceBoard} (Auto Detect)`].concat(state.scanData.supportedBoards);
  const values = [''].concat(state.scanData.supportedBoards);
  renderSelect(ele, 'board', 'Reference Board Name', keys, values);
  renderButton(ele, 'Extract', handleExtract);
  renderText(ele, 'Warning: Choose wrong board may damage the hardware!');
  renderTable(ele, state.extractData);
};
const handleLock = async () => {
  setStateAndRender({isLoading: true, message: 'Locking...'});
  const resp = await fetchAPI('/lock', {cr50SerialName: state.scanData.cr50SerialName});
  const data = await resp.json();
  if (data.success) {
    /* Re-scan the device */
    await handleScan();
  } else {
    setStateAndRender({isLoading: false, message: 'Lock failed.'});
  }
};
const renderLock = (ele) => {
  renderText(ele, 'Lock the device', 'h4');
  renderButton(ele, 'Lock', handleLock);
};
const render = () => {
  const root = document.getElementById('root');
  while (root.firstChild) {
    root.removeChild(root.lastChild);
  }
  renderText(root, 'HWID Extractor', 'h3');
  if (state.message) {
    renderText(root, state.message, 'p');
  }
  renderScan(root);
  if (!state.scanData)
    return;
  if (state.scanData.isRestricted) {
    renderUnlock(root);
  } else {
    renderExtract(root);
    renderLock(root);
  }
};
window.onload = async () => {
  render();
};
