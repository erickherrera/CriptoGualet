-- CriptoGualet Database Schema Migrations
-- This file contains all database schema versions for the cryptocurrency wallet

-- ============================================================================
-- MIGRATION VERSION 1: Initial Schema
-- Description: Create core tables for users, wallets, addresses, and transactions
-- ============================================================================

-- Users table - stores user authentication data
CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,
    salt BLOB NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_login TIMESTAMP,
    wallet_version INTEGER DEFAULT 1,
    is_active BOOLEAN DEFAULT 1,

    -- Constraints
    CHECK (length(username) >= 3),
    CHECK (length(password_hash) > 0),
    CHECK (length(salt) >= 16)
);

-- Wallets table - supports multiple wallets per user
CREATE TABLE IF NOT EXISTS wallets (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL,
    wallet_name TEXT NOT NULL,
    wallet_type TEXT DEFAULT 'bitcoin',
    derivation_path TEXT, -- BIP44 derivation path (e.g., m/44'/0'/0')
    extended_public_key TEXT, -- Master public key for address generation
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    is_active BOOLEAN DEFAULT 1,

    -- Foreign key constraint
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,

    -- Constraints
    CHECK (wallet_type IN ('bitcoin', 'bitcoin_testnet', 'litecoin', 'ethereum')),
    CHECK (length(wallet_name) > 0),

    -- Unique constraint: one wallet name per user
    UNIQUE (user_id, wallet_name)
);

-- Addresses table - generated addresses for each wallet
CREATE TABLE IF NOT EXISTS addresses (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    wallet_id INTEGER NOT NULL,
    address TEXT UNIQUE NOT NULL,
    address_index INTEGER NOT NULL,
    is_change BOOLEAN DEFAULT 0, -- 0 = receiving address, 1 = change address
    public_key TEXT, -- Public key for this address
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    label TEXT, -- User-defined label for the address
    balance_satoshis INTEGER DEFAULT 0, -- Cached balance

    -- Foreign key constraint
    FOREIGN KEY (wallet_id) REFERENCES wallets(id) ON DELETE CASCADE,

    -- Constraints
    CHECK (address_index >= 0),
    CHECK (balance_satoshis >= 0),
    CHECK (length(address) > 0),

    -- Unique constraint: one address per index per wallet
    UNIQUE (wallet_id, address_index, is_change)
);

-- Transactions table - stores wallet transaction history
CREATE TABLE IF NOT EXISTS transactions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    wallet_id INTEGER NOT NULL,
    txid TEXT UNIQUE NOT NULL,
    block_height INTEGER,
    block_hash TEXT,
    amount_satoshis INTEGER NOT NULL,
    fee_satoshis INTEGER DEFAULT 0,
    direction TEXT NOT NULL, -- 'incoming', 'outgoing', 'internal'
    from_address TEXT,
    to_address TEXT,
    confirmation_count INTEGER DEFAULT 0,
    is_confirmed BOOLEAN DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    confirmed_at TIMESTAMP,
    memo TEXT, -- User note for the transaction

    -- Foreign key constraint
    FOREIGN KEY (wallet_id) REFERENCES wallets(id) ON DELETE CASCADE,

    -- Constraints
    CHECK (direction IN ('incoming', 'outgoing', 'internal')),
    CHECK (confirmation_count >= 0),
    CHECK (fee_satoshis >= 0),
    CHECK (length(txid) > 0)
);

-- Transaction inputs table - detailed input information
CREATE TABLE IF NOT EXISTS transaction_inputs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    transaction_id INTEGER NOT NULL,
    input_index INTEGER NOT NULL,
    prev_txid TEXT NOT NULL,
    prev_output_index INTEGER NOT NULL,
    script_sig TEXT,
    sequence INTEGER DEFAULT 4294967295,
    address TEXT,
    amount_satoshis INTEGER,

    -- Foreign key constraint
    FOREIGN KEY (transaction_id) REFERENCES transactions(id) ON DELETE CASCADE,

    -- Constraints
    CHECK (input_index >= 0),
    CHECK (prev_output_index >= 0),
    CHECK (amount_satoshis >= 0),

    -- Unique constraint
    UNIQUE (transaction_id, input_index)
);

-- Transaction outputs table - detailed output information
CREATE TABLE IF NOT EXISTS transaction_outputs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    transaction_id INTEGER NOT NULL,
    output_index INTEGER NOT NULL,
    script_pubkey TEXT,
    address TEXT,
    amount_satoshis INTEGER NOT NULL,
    is_spent BOOLEAN DEFAULT 0,
    spent_in_txid TEXT, -- Reference to transaction that spent this output

    -- Foreign key constraint
    FOREIGN KEY (transaction_id) REFERENCES transactions(id) ON DELETE CASCADE,

    -- Constraints
    CHECK (output_index >= 0),
    CHECK (amount_satoshis >= 0),

    -- Unique constraint
    UNIQUE (transaction_id, output_index)
);

-- Encrypted seeds table - stores encrypted mnemonic seeds
CREATE TABLE IF NOT EXISTS encrypted_seeds (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER UNIQUE NOT NULL,
    encrypted_seed BLOB NOT NULL, -- AES encrypted seed
    encryption_salt BLOB NOT NULL, -- Salt for seed encryption
    key_derivation_iterations INTEGER DEFAULT 100000, -- PBKDF2 iterations
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    backup_confirmed BOOLEAN DEFAULT 0, -- User confirmed backup

    -- Foreign key constraint
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,

    -- Constraints
    CHECK (length(encrypted_seed) > 0),
    CHECK (length(encryption_salt) >= 16),
    CHECK (key_derivation_iterations >= 10000)
);

-- Settings table - application configuration
CREATE TABLE IF NOT EXISTS settings (
    key TEXT PRIMARY KEY,
    value TEXT,
    data_type TEXT DEFAULT 'string', -- 'string', 'integer', 'boolean', 'json'
    description TEXT,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,

    -- Constraints
    CHECK (data_type IN ('string', 'integer', 'boolean', 'json')),
    CHECK (length(key) > 0)
);

-- Address book table - user's contact addresses
CREATE TABLE IF NOT EXISTS address_book (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL,
    name TEXT NOT NULL,
    address TEXT NOT NULL,
    address_type TEXT DEFAULT 'bitcoin',
    notes TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,

    -- Foreign key constraint
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,

    -- Constraints
    CHECK (length(name) > 0),
    CHECK (length(address) > 0),
    CHECK (address_type IN ('bitcoin', 'bitcoin_testnet', 'litecoin', 'ethereum')),

    -- Unique constraint: one name per user
    UNIQUE (user_id, name)
);

-- ============================================================================
-- INDEXES for Performance
-- ============================================================================

-- Users table indexes
CREATE INDEX IF NOT EXISTS idx_users_username ON users(username);
CREATE INDEX IF NOT EXISTS idx_users_created_at ON users(created_at);

-- Wallets table indexes
CREATE INDEX IF NOT EXISTS idx_wallets_user_id ON wallets(user_id);
CREATE INDEX IF NOT EXISTS idx_wallets_type ON wallets(wallet_type);

-- Addresses table indexes
CREATE INDEX IF NOT EXISTS idx_addresses_wallet_id ON addresses(wallet_id);
CREATE INDEX IF NOT EXISTS idx_addresses_address ON addresses(address);
CREATE INDEX IF NOT EXISTS idx_addresses_balance ON addresses(balance_satoshis);

-- Transactions table indexes
CREATE INDEX IF NOT EXISTS idx_transactions_wallet_id ON transactions(wallet_id);
CREATE INDEX IF NOT EXISTS idx_transactions_txid ON transactions(txid);
CREATE INDEX IF NOT EXISTS idx_transactions_block_height ON transactions(block_height);
CREATE INDEX IF NOT EXISTS idx_transactions_direction ON transactions(direction);
CREATE INDEX IF NOT EXISTS idx_transactions_confirmed ON transactions(is_confirmed);
CREATE INDEX IF NOT EXISTS idx_transactions_created_at ON transactions(created_at);

-- Transaction inputs/outputs indexes
CREATE INDEX IF NOT EXISTS idx_tx_inputs_transaction_id ON transaction_inputs(transaction_id);
CREATE INDEX IF NOT EXISTS idx_tx_inputs_prev_txid ON transaction_inputs(prev_txid);
CREATE INDEX IF NOT EXISTS idx_tx_outputs_transaction_id ON transaction_outputs(transaction_id);
CREATE INDEX IF NOT EXISTS idx_tx_outputs_address ON transaction_outputs(address);
CREATE INDEX IF NOT EXISTS idx_tx_outputs_spent ON transaction_outputs(is_spent);

-- Address book indexes
CREATE INDEX IF NOT EXISTS idx_address_book_user_id ON address_book(user_id);
CREATE INDEX IF NOT EXISTS idx_address_book_address ON address_book(address);

-- ============================================================================
-- DEFAULT SETTINGS
-- ============================================================================

-- Insert default application settings
INSERT OR IGNORE INTO settings (key, value, data_type, description) VALUES
('app_version', '1.0.0', 'string', 'Application version'),
('database_version', '1', 'integer', 'Database schema version'),
('default_wallet_type', 'bitcoin', 'string', 'Default wallet type for new wallets'),
('auto_backup_enabled', 'true', 'boolean', 'Enable automatic database backups'),
('backup_retention_days', '30', 'integer', 'Number of days to keep backup files'),
('transaction_cache_hours', '24', 'integer', 'Hours to cache transaction data'),
('address_gap_limit', '20', 'integer', 'Maximum gap in address sequence'),
('min_confirmations', '6', 'integer', 'Minimum confirmations for confirmed transactions'),
('fee_estimation_blocks', '6', 'integer', 'Blocks to target for fee estimation'),
('max_transaction_history', '1000', 'integer', 'Maximum transactions to keep in memory');

-- ============================================================================
-- TRIGGERS for Data Integrity
-- ============================================================================

-- Update last_login when user authenticates
CREATE TRIGGER IF NOT EXISTS update_user_last_login
    AFTER UPDATE OF password_hash ON users
    WHEN NEW.password_hash = OLD.password_hash
BEGIN
    UPDATE users SET last_login = CURRENT_TIMESTAMP WHERE id = NEW.id;
END;

-- Update address balance when transactions change
CREATE TRIGGER IF NOT EXISTS update_address_balance_on_tx_insert
    AFTER INSERT ON transactions
BEGIN
    -- Update receiving address balance
    UPDATE addresses
    SET balance_satoshis = balance_satoshis +
        CASE WHEN NEW.direction = 'incoming' THEN NEW.amount_satoshis ELSE 0 END
    WHERE address = NEW.to_address AND wallet_id = NEW.wallet_id;

    -- Update sending address balance
    UPDATE addresses
    SET balance_satoshis = balance_satoshis -
        CASE WHEN NEW.direction = 'outgoing' THEN NEW.amount_satoshis + NEW.fee_satoshis ELSE 0 END
    WHERE address = NEW.from_address AND wallet_id = NEW.wallet_id;
END;

-- ============================================================================
-- VIEWS for Common Queries
-- ============================================================================

-- View for wallet summary with balances
CREATE VIEW IF NOT EXISTS wallet_summary AS
SELECT
    w.id as wallet_id,
    w.user_id,
    w.wallet_name,
    w.wallet_type,
    w.created_at,
    COUNT(DISTINCT a.id) as address_count,
    COALESCE(SUM(a.balance_satoshis), 0) as total_balance_satoshis,
    COUNT(DISTINCT t.id) as transaction_count,
    MAX(t.created_at) as last_transaction_date
FROM wallets w
LEFT JOIN addresses a ON w.id = a.wallet_id
LEFT JOIN transactions t ON w.id = t.wallet_id
WHERE w.is_active = 1
GROUP BY w.id, w.user_id, w.wallet_name, w.wallet_type, w.created_at;

-- View for recent transactions with full details
CREATE VIEW IF NOT EXISTS recent_transactions AS
SELECT
    t.id,
    t.wallet_id,
    w.wallet_name,
    t.txid,
    t.amount_satoshis,
    t.fee_satoshis,
    t.direction,
    t.from_address,
    t.to_address,
    t.confirmation_count,
    t.is_confirmed,
    t.created_at,
    t.confirmed_at,
    t.memo
FROM transactions t
JOIN wallets w ON t.wallet_id = w.id
ORDER BY t.created_at DESC;