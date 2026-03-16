import os
import stat

def make_writable_and_delete(path):
    try:
        # Remove read-only attribute if set
        os.chmod(path, stat.S_IWRITE)
        os.remove(path)
        print(f"Deleted: {path}")
        return True
    except Exception as e:
        print(f"Failed to delete {path}: {e}")
        return False

deleted_count = 0
for root, dirs, files in os.walk('.'):
    dirs[:] = [d for d in dirs if d.lower() != 'intermediate']
    for fname in files:
        if fname.endswith('.bak'):
            path = os.path.join(root, fname)
            if make_writable_and_delete(path):
                deleted_count += 1

print(f"\nCompleted. Total .bak files deleted: {deleted_count}")
