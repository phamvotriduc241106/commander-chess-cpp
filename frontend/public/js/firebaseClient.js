import { getApp, getApps, initializeApp } from 'https://www.gstatic.com/firebasejs/11.10.0/firebase-app.js';
import {
  GoogleAuthProvider,
  getAuth,
  linkWithPopup,
  onAuthStateChanged,
  signInAnonymously,
  signInWithPopup,
  signOut
} from 'https://www.gstatic.com/firebasejs/11.10.0/firebase-auth.js';
import {
  addDoc,
  collection,
  doc,
  getDoc,
  getDocs,
  getFirestore,
  limit,
  onSnapshot,
  orderBy,
  query,
  runTransaction,
  serverTimestamp,
  setDoc,
  updateDoc,
  where
} from 'https://www.gstatic.com/firebasejs/11.10.0/firebase-firestore.js';

const DEFAULT_FIREBASE_CONFIG = {
  apiKey: 'AIzaSyBiXLUvMjKV5Mppi8yWHoVDSKnc6i65eO8',
  authDomain: 'commander-chess-cpp-1771534802.firebaseapp.com',
  projectId: 'commander-chess-cpp-1771534802',
  storageBucket: 'commander-chess-cpp-1771534802.firebasestorage.app',
  messagingSenderId: '969532280338',
  appId: '1:969532280338:web:b11eca1a31ea706d3812cb'
};

function readFirebaseConfig() {
  const fromWindow = (
    typeof window !== 'undefined' &&
    window.__COMMANDER_FIREBASE_CONFIG &&
    typeof window.__COMMANDER_FIREBASE_CONFIG === 'object'
  ) ? window.__COMMANDER_FIREBASE_CONFIG : null;

  if (!fromWindow) return { ...DEFAULT_FIREBASE_CONFIG };
  return { ...DEFAULT_FIREBASE_CONFIG, ...fromWindow };
}

const firebaseConfig = readFirebaseConfig();
const googleSignInEnabled = Boolean(
  typeof window === 'undefined' ||
  window.__COMMANDER_ENABLE_GOOGLE_AUTH !== false
);
const firebaseReady = Boolean(
  firebaseConfig.apiKey &&
  firebaseConfig.authDomain &&
  firebaseConfig.projectId &&
  firebaseConfig.appId
);

const app = firebaseReady
  ? (getApps().length ? getApp() : initializeApp(firebaseConfig))
  : null;
const auth = app ? getAuth(app) : null;
const db = app ? getFirestore(app) : null;

function assertFirebaseReady() {
  if (!firebaseReady || !auth || !db) {
    throw new Error(
      'Firebase is not configured. Set window.__COMMANDER_FIREBASE_CONFIG with web SDK keys.'
    );
  }
}

async function ensureSignedIn() {
  assertFirebaseReady();
  if (auth.currentUser) return auth.currentUser;
  const cred = await signInAnonymously(auth);
  return cred.user;
}

async function signInWithGoogle() {
  assertFirebaseReady();
  const provider = new GoogleAuthProvider();
  provider.setCustomParameters({ prompt: 'select_account' });

  if (auth.currentUser && auth.currentUser.isAnonymous) {
    try {
      const linked = await linkWithPopup(auth.currentUser, provider);
      return linked.user;
    } catch (err) {
      // If linking fails due to existing credential, fallback to direct sign-in.
      const code = err && err.code ? String(err.code) : '';
      if (
        code === 'auth/credential-already-in-use' ||
        code === 'auth/email-already-in-use' ||
        code === 'auth/provider-already-linked'
      ) {
        const signedIn = await signInWithPopup(auth, provider);
        return signedIn.user;
      }
      throw err;
    }
  }

  const cred = await signInWithPopup(auth, provider);
  return cred.user;
}

async function signOutUser() {
  assertFirebaseReady();
  await signOut(auth);
}

function onAuthState(listener) {
  if (typeof listener !== 'function') return () => {};
  if (!firebaseReady || !auth) {
    listener(null);
    return () => {};
  }
  return onAuthStateChanged(auth, listener);
}

export {
  addDoc,
  app,
  assertFirebaseReady,
  auth,
  collection,
  db,
  doc,
  ensureSignedIn,
  firebaseConfig,
  firebaseReady,
  googleSignInEnabled,
  getDoc,
  getDocs,
  limit,
  onAuthState,
  onSnapshot,
  orderBy,
  query,
  runTransaction,
  serverTimestamp,
  setDoc,
  signInWithGoogle,
  signOutUser,
  updateDoc,
  where
};
